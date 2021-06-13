Instructions
============

Write a TCP ECHO server with event loop (select/poll/epoll/etc.). The server
should read newline (LF) separated data from client, and echo it back.
Data must be sent line buffered, so each line can be echoed back after
LF has been read for that line.

The server has to be able to handle multiple concurrent clients, and threads
cannot be used. You might have to read multiple times before reading newline.

Target platform for the software is Linux.

Use the provided Makefile to compile your software, you can amend it to suite
multiple objects if you want to split your code.

Solution:
============
solution: Single threaded echo server - with epoll I/O events 
compile : make all
Usage   : ./epollEchoServer <port#>
