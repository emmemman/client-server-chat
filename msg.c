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
    size_t sender_len = strlen(sender);
    size_t msg_len   = strlen(msg);
    uint32_t net_sender_len = htonl(sender_len);
    uint32_t net_msg_len    = htonl(msg_len);

    // Send sender length
    if (writeall(fd, &net_sender_len, sizeof(net_sender_len)) == -1)
        return -1;

    // Send sender name
    if (writeall(fd, sender, sender_len) == -1)
        return -1;

    // Send message length
    if (writeall(fd, &net_msg_len, sizeof(net_msg_len)) == -1)
        return -1;

    // Send message content
    if (writeall(fd, msg, msg_len) == -1)
        return -1;

    // Return total number of payload bytes sent (sender + message)
    return (ssize_t)(sender_len + msg_len);
}

ssize_t recvNewMessage(int fd, char *msg, size_t msg_max_len,
                       char *sender, size_t sender_max_len)
{
    ssize_t nread;
    uint32_t net_sender_len, sender_len;
    uint32_t net_msg_len, msg_len;

    // 1. Read sender length header
    nread = readall(fd, &net_sender_len, sizeof(net_sender_len));
    if (nread == 0 || nread == -1)
        return nread;

    sender_len = ntohl(net_sender_len);

    // Validate sender length against provided buffer
    if (sender_len >= sender_max_len)
        return -2;   // sender too long for buffer

    // Optional: global upper bound (you may define MAX_NAME_LEN)
    // if (sender_len > MAX_NAME_LEN) return -3;

    // 2. Read sender string
    nread = readall(fd, sender, sender_len);
    if (nread == 0 || nread == -1)
        return nread;
    sender[sender_len] = '\0';

    // 3. Read message length header
    nread = readall(fd, &net_msg_len, sizeof(net_msg_len));
    if (nread == 0 || nread == -1)
        return nread;

    msg_len = ntohl(net_msg_len);

    // Validate message length against buffer and global limit
    if (msg_len >= msg_max_len)
        return -2;   // message too long for buffer

    // if (msg_len > MAX_MSG_LEN) return -3;

    // 4. Read message string
    nread = readall(fd, msg, msg_len);
    if (nread == 0 || nread == -1)
        return nread;
    msg[msg_len] = '\0';

    // Return number of bytes read for the message payload
    return nread;
}