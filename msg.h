#include <sys/types.h>

ssize_t sendMessage(int fd, const char *msg);
ssize_t recvMessage(int fd, char *msg, size_t max_len);
ssize_t sendNewMessage(int fd, const char *msg, const char *sender);
ssize_t recvNewMessage(int fd, char *msg, size_t msg_max_len, char *sender, size_t sender_max_len);