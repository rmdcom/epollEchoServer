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

// function declarations.
int setup_serverSocket (char *port);

// Main Program
int main (int argc, char *argv[])
{
  int sfd, s, efd;
  struct epoll_event event;
  struct epoll_event *events;
  std::map<int,int> clientMap;

  if (argc != 2) {
      fprintf (stderr, "Usage: %s [port]\n", argv[0]);
      exit (EXIT_FAILURE);
  }

  // initialize the server socket with port
  sfd = setup_serverSocket(argv[1]);

  if (sfd == -1) abort ();

  // set socket fd to be non-blocking mode
  int flags = fcntl (sfd, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl (sfd, F_SETFL, flags);

  // listen socket as passive mode
  s = listen (sfd, SOMAXCONN);   
  if (s == -1)
    {
      perror ("listen() failed !");
      abort ();
    }

  // create epoll instance
  efd = epoll_create1 (0); 
  if (efd == -1)
    {
      perror ("epoll_create() failed !");
      abort ();
    }

  // make read events using edge triggered mode
  event.data.fd = sfd;
  event.events = EPOLLIN | EPOLLET;

  // Add server socket FD to epoll's watched list
  s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event); 
  if (s == -1)
    {
      perror ("epoll_ctl() failed!");
      abort ();
    }

  /* Events buffer used by epoll_wait to list triggered events */
  events = (epoll_event*) calloc (MAX_EVENTS, sizeof(event));  

  /* The worked event loop */
  while (1)
    {
      int n, i;
      // Block until some events appears, no timeout (-1)
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

                  s = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event); 
                  if (s == -1)
                    {
                      perror ("epoll_ctl() failed !");
                      abort ();
                    }
                  clientMap[event.data.fd]=0;  // init msg counter    
                }
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
              int done = 0;

              while (1)
                {
                  ssize_t count;
                  char buf[BUFFER_SIZE];

                  count = read (events[i].data.fd, buf, sizeof buf);  
                  
                  if (count == -1)
                    {
                      /* If errno == EAGAIN, that means we have read all
                         data. So go back to the main loop. */
                      if (errno != EAGAIN)
                        {
                          perror ("read() failed !");
                          done = 1;
                        }
                      break;
                    }
                  else if (count == 0)
                    {
                      /* End of file. The remote has closed the
                         connection. */
                      done = 1;
                      break;
                    }
                  
                  // ToDo
                  buf[count]=0;
                  char wbuf[BUFFER_SIZE];
                  int cx=snprintf(wbuf,BUFFER_SIZE,"(FD:%d SEQ:%d) %s",events[i].data.fd, clientMap[events[i].data.fd],buf);

                  /* Write the buffer to standard output (1) and echo back to client fd sdf */
                  s = write (1, wbuf, cx);
                  if (s == -1)
                    {
                      perror ("write failed !");
                      abort ();
                    }
                }
                // Increment msg counter local ds
                int tmp = clientMap[events[i].data.fd];
                tmp++;
                clientMap[events[i].data.fd]=tmp;
                
              if (done)
                {
                  printf ("Closed connection on descriptor %d\n",
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


// functions definations
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
