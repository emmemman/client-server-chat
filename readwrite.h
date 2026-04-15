#include <sys/types.h>

ssize_t writeall(int fd, const void *buf, size_t nbyte);
ssize_t readall(int fd, void *buf, size_t nbyte);