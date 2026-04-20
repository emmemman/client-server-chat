#ifndef HISTORY_H
#define HISTORY_H

#include <pthread.h>
#include "config.h"

#define MAX_HISTORY 100   // Keep the last 100 messages

// A single history entry
struct history_entry {
    char sender[MAX_NAME_LEN + 1];
    char msg[MAX_MSG_LEN + 1];
};

// The history structure (circular buffer)
struct message_history {
    struct history_entry entries[MAX_HISTORY];
    int start;   // index of oldest message
    int count;   // number of stored messages
    pthread_mutex_t lock;
};

// Initialize the history
void init_history(struct message_history *hist);

// Add a new message to the history (thread‑safe)
void add_history(struct message_history *hist, const char *sender, const char *msg);

// Send the entire history to a new client (thread‑safe)
void send_history(int client_fd, struct message_history *hist);

// Clean up (not strictly needed for static allocation)
void destroy_history(struct message_history *hist);

#endif