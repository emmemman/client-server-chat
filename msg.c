#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#include <stdio.h>

#include "readwrite.h"
#include "config.h"

ssize_t sendMessage(int fd, const char *msg)
{
	size_t msg_len = strlen(msg);
	uint32_t net_msg_len = htonl(msg_len);

	if (writeall(fd, &net_msg_len, sizeof(net_msg_len)) == -1) {
		return -1;
	}

	return writeall(fd, msg, msg_len);
}

ssize_t recvMessage(int fd, char *msg, size_t max_len)
{
	ssize_t nread;
	uint32_t net_msg_len, msg_len;

	nread = readall(fd, &net_msg_len, sizeof(net_msg_len));
	if (nread == 0 || nread == -1) {
		return nread;
	}

	msg_len = ntohl(net_msg_len);

	if (msg_len >= max_len) {
		return -2;
	}

	if (msg_len > MAX_MSG_LEN) {
		return -3;
	}

	nread = readall(fd, msg, msg_len);
	if (nread == 0 || nread == -1) {
		return nread;
	}

	msg[nread] = '\0';

	return nread;
}

ssize_t sendNewMessage(int fd, const char *msg, const char *sender)
{
	(void) sender;

	//TODO: send the sender's name first

	return sendMessage(fd, msg);
}

ssize_t recvNewMessage(int fd, char *msg, size_t msg_max_len, char *sender, size_t sender_max_len)
{
	(void) sender;
	(void) sender_max_len;

	//TODO: read the sender's name first

	strcpy(sender, "TODO: READ THE SENDER's NAME");

	return recvMessage(fd, msg, msg_max_len);
}