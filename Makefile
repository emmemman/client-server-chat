CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2
LDFLAGS = -pthread

all: client server

client: client.c readwrite.c msg.c printMsg.c config.h readwrite.h msg.h printMsg.h
	$(CC) $(CFLAGS) -o $@ client.c readwrite.c msg.c printMsg.c $(LDFLAGS)

server: server.c readwrite.c msg.c clientsList.c printMsg.c history.c config.h readwrite.h msg.h clientsList.h printMsg.h history.h
	$(CC) $(CFLAGS) -o $@ server.c readwrite.c msg.c clientsList.c printMsg.c history.c $(LDFLAGS)

clean:
	rm -f client server

.PHONY: all clean