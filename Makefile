CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2
LDFLAGS = -pthread

all: client server

client: client.c msg.c config.h readwrite.h msg.h printMsg.h
	$(CC) $(CFLAGS) -o $@ client.c readwrite.c msg.c clientsList.c printMsg.c $(LDFLAGS)

server: server.c msg.c clientsList.c config.h readwrite.h msg.h clientsList.h printMsg.h
	$(CC) $(CFLAGS) -o $@ server.c readwrite.c msg.c clientsList.c printMsg.c $(LDFLAGS)

clean:
	rm -f client server

.PHONY: all clean
