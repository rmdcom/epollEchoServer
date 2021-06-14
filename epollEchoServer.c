#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netdb.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<map>

// Macro defination
#define BUFFER_SIZE 4096
#define MAX_EVENTS  1024
#define SOCKET_ERROR (-1)
std::map<int,int> clientMap;

// function declarations.
int setup_serverSocket (char *port);
int  fail_check (int exp, const char *msg);
int accept_new_connection(int sfd, int efd, epoll_event event);
// Main Program
int main (int argc, char *argv[])
{
  int sfd, s, efd;
  struct epoll_event event;
  struct epoll_event *events;

  if (argc != 2) {
      fprintf (stderr, "Usage: %s [port]\n", argv[0]);
      exit (EXIT_FAILURE);
  }

  // initialize the server socket with port
  sfd = setup_serverSocket(argv[1]);
  fail_check(sfd, "socket init failed!");

  // set socket fd to be non-blocking mode
  int flags = fcntl (sfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl (sfd, F_SETFL, flags);

  // listen socket as passive mode
  fail_check (listen (sfd, SOMAXCONN), "listen() failed !");   
  
  // create epoll instance
  efd = epoll_create1 (0); 
  fail_check (efd,"epoll_create() failed !");
  
  // make read events using edge triggered mode
  event.data.fd = sfd;
  event.events = EPOLLIN | EPOLLET;

  // Add server socket FD to epoll's watched list
  
  fail_check( (epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event)), "epoll_ctl() failed!" ); 
  
  /* Events buffer used by epoll_wait to list triggered events */
  events = (epoll_event*) calloc (MAX_EVENTS, sizeof(event));  

  /* The worked event loop */
  while (1)
    {
      int n, i;
      // Block until some events appears, no timeout (-1)
       printf("\n Wait for connections...\n");
      n = epoll_wait (efd, events, MAX_EVENTS, -1);
      for (i = 0; i < n; i++)
	      {
          /* An error has occured on this fd, or the socket is not ready for reading*/    
          if ((events[i].events & EPOLLERR) ||
            (events[i].events & EPOLLHUP) ||
            (!(events[i].events & EPOLLIN)))
	          {
              fprintf (stderr, "epoll error\n");
	            close (events[i].data.fd);  // Closing the fd removes from the epoll monitored list
              clientMap.erase(events[i].data.fd);
	            continue;
	          }
	        /* server socket accepting new connections */
          else if (sfd == events[i].data.fd)
	          {
              accept_new_connection(sfd, efd, event);
              continue;
            }
          else
            {
              /* We have data on the fd waiting to be read. 
                  Read and echo back to client. 
                 We shall read each line with line buffered LF
                  as we are running in edge-triggered mode
                  and won't get a notification again for the same data. 
              */
              int running = 0;

              while (1)
                {
                  ssize_t count;
                  char buffer[BUFFER_SIZE];
                  
                  // Read the complete client message
                  count = read (events[i].data.fd, buffer, sizeof(buffer));  

                  /* If errno == EAGAIN, that means we have read all data. So go back to the main loop. */
                  if (count == -1)
                    {
                      if (errno != EAGAIN)
                        {
                          perror ("read() failed !");
                          running = 1;
                        }
                      break;
                    }
                  /* End of file. The remote has closed the connection. */
                  else if (count == 0)
                    {
                      running = 1;
                      break;
                    }
                  
                  // ToDo: Handle the bussiness Logic for requirement
                  // Tokanize the full client message with LF buffered and echo/write back to client
                
                  // null termimnate the message and remove the \n
                  buffer[count]=0;

                  printf("REQUEST:%s\n",buffer);  
                  /* Write the buffer echo back to client fd sdf */

                  char tmp_buf[BUFFER_SIZE];                  
                  char* token;
                  token = strtok(buffer,"\n");
                  while(token != NULL)
                  {

                    printf("Tokens: %s\n",token);
                    int cx=sprintf(tmp_buf,"%s",token);
                    tmp_buf[cx]=0;
                    s = write (events[i].data.fd, tmp_buf, cx);
                    if (s == -1)
                    {
                      perror ("write failed !");
                      abort ();
                    }
                    token = strtok(NULL,"\n");
                  }
                }
                // Increment msg counter local ds
                int tmp = clientMap[events[i].data.fd];
                tmp++;
                clientMap[events[i].data.fd]=tmp;
                
              if (running)
                {
                  printf ("\n Closed connection on descriptor %d\n",
                          events[i].data.fd);

                  /* Closing the descriptor will make epoll remove it
                     from the set of descriptors which are monitored. */
                  close (events[i].data.fd);
                  clientMap.erase(events[i].data.fd);
                }
            }
        }
    }

  free (events);
  close (sfd);

  return EXIT_SUCCESS;
}


// public functions definations

int setup_serverSocket (char *port)
{
  struct addrinfo hints, *res;
  int status, sfd;

  // initilize the server socket
  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = AF_INET;                    // IPV4   
  hints.ai_socktype = SOCK_STREAM;              // TCP socket 
  hints.ai_flags = AI_PASSIVE;                  

  // get and update client details for socket call
  status = getaddrinfo (NULL, port, &hints, &res);
  if (status != 0)
  {
    fprintf (stderr, "getaddrinfo error: %s\n", gai_strerror (status));
    return -1;
  }
  
  // create socket file descriptor
  sfd = socket (res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sfd == -1) {
    fprintf (stderr, "socket error\n");
    close (sfd); 
    return -1;
  }

  // port reuse option
  int optval = 1;
  setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

  // bind address (INADDR_ANY) to socket file descriptor.
  status = bind (sfd, res->ai_addr, res->ai_addrlen);  
  if (status == -1)
  {
    fprintf (stderr, "bind failed\n");
    return -1;      
  }  

  freeaddrinfo (res);
  return sfd;
}

int accept_new_connection(int sfd, int efd, epoll_event event)
{
  int s;
  while (1)
  {
    struct sockaddr in_addr;
    socklen_t in_len;
    int infd;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    in_len = sizeof in_addr;
    // create new socket fd from pending listening sockets in queue
    infd = accept (sfd, &in_addr, &in_len); 
    if (infd == -1) // error
      {
        if ((errno == EAGAIN) ||
            (errno == EWOULDBLOCK))
          {
            /* We have processed all incoming connections. */
            break;
          }
        else
          {
            perror ("accept failed !");
            break;
          }
      }

    int optval = 1;
    // set socket for port reuse
    setsockopt(infd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    
    /* get the client's IP addr and port num */
    s = getnameinfo (&in_addr, in_len,
                      hbuf, sizeof hbuf,
                      sbuf, sizeof sbuf,
                      NI_NUMERICHOST | NI_NUMERICSERV);
    if (s == 0)
      {
        printf("Accepted connection on FD %d "
                "(host=%s, port=%s)\n", infd, hbuf, sbuf);
      }

    /* Make the incoming socket non-blocking and add it to the
        list of fds to monitor. */        
    int flags = fcntl (infd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl (infd, F_SETFL, flags);

    event.data.fd = infd;
    event.events = EPOLLIN | EPOLLET;                  

    fail_check ( (epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event)), "epoll_ctl() failed !"); 
    
    clientMap[event.data.fd]=0;  // init msg counter    
  }
  return sfd;
}
int  fail_check (int exp, const char *msg)
{
    if (exp == SOCKET_ERROR){
        perror(msg);
        exit(1);
    }   
    return exp;
}
