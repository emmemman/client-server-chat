#include <stdio.h>

#include "clientsList.h"
#include "msg.h"

void initClientsList(struct clientsList *cl)
{
	cl->clients_num = 0;
	pthread_mutex_init(&cl->clients_lock, NULL);
}

int addClient(struct clientsList *cl, int fd)
{
	pthread_mutex_lock(&cl->clients_lock);
	if (cl->clients_num == MAX_CONNECTED_CLIENTS) {
			pthread_mutex_unlock(&cl->clients_lock);
			return -1;
	}
	cl->clients_fd[cl->clients_num++] = fd;
	pthread_mutex_unlock(&cl->clients_lock);
	return 0;
}

int removeClient(struct clientsList *cl, int fd)
{
	int removed = 0;
	int i = 0;
	int j;

	pthread_mutex_lock(&cl->clients_lock);
	while (!removed && i < cl->clients_num) {
		if (cl->clients_fd[i] == fd) {
			for (j = i + 1; j < cl->clients_num; ++j) {
				cl->clients_fd[j - 1] = cl->clients_fd[j];
			}
			removed = 1;
			cl->clients_num--;
		}
		++i;
	}
	pthread_mutex_unlock(&cl->clients_lock);

	if (!removed)
		return -1;

	return 0;
}

int broadcastClientsList(struct clientsList *cl, const char *msg, const char *sender, int sender_fd) {

	(void) cl;
	(void) msg;
	(void) sender;
	(void) sender_fd;

	//TODO use sendNewMessage to broadcast the message to all clients except the sender of the message

	return 0;
}

void destroyClientsList(struct clientsList *cl)
{
	cl->clients_num = 0;
	pthread_mutex_destroy(&cl->clients_lock);
}
