#include <pthread.h>
#include <sys/types.h>

#define MAX_CONNECTED_CLIENTS 100

struct clientsList {
	int clients_num;
	int clients_fd[MAX_CONNECTED_CLIENTS];
	pthread_mutex_t clients_lock;
};

void initClientsList(struct clientsList *c);
int addClient(struct clientsList *c, int fd);
int removeClient(struct clientsList *c, int fd);
int broadcastClientsList(struct clientsList *cl, const char *msg, const char *sender, int sender_fd);
void destroyClientsList(struct clientsList *c);