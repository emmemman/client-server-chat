#ifndef HISTORY_H
#define HISTORY_H

#include <pthread.h>
#include "config.h"

#define MAX_HISTORY 100 

struct history_entry {
    char sender[MAX_NAME_LEN + 1];
    char msg[MAX_MSG_LEN + 1];
};

struct message_history {
    struct history_entry entries[MAX_HISTORY];
    int start; 
    int count;   
    pthread_mutex_t lock;
};

void init_history(struct message_history *hist);

void add_history(struct message_history *hist, const char *sender, const char *msg);

void send_history(int client_fd, struct message_history *hist);

void destroy_history(struct message_history *hist);

#endif