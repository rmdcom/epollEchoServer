CC=g++
CFLAGS=-ggdb3 -O0 -W -Wall -Werror
LD=g++
BIN=epollEchoServer
OBJECTS=epollEchoServer.o
LIBS=
LDFLAGS=

.phony: all clean

.c.o:
	$(CC) -c $< -o $@ $(CFLAGS)

echo: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

clean:
	rm -rf $(OBJECTS) $(BIN)

all: $(BIN)
