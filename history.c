#include "history.h"
#include "msg.h"
#include <string.h>
#include <stdio.h>

void init_history(struct message_history *hist) {
    hist->start = 0;
    hist->count = 0;
    pthread_mutex_init(&hist->lock, NULL);
}

void add_history(struct message_history *hist, const char *sender, const char *msg) {
    pthread_mutex_lock(&hist->lock);

    int idx;
    if (hist->count < MAX_HISTORY) {
        // Buffer not yet full – append at end
        idx = (hist->start + hist->count) % MAX_HISTORY;
        hist->count++;
    } else {
        // Buffer full – overwrite the oldest entry
        idx = hist->start;
        hist->start = (hist->start + 1) % MAX_HISTORY;
    }

    strncpy(hist->entries[idx].sender, sender, MAX_NAME_LEN);
    hist->entries[idx].sender[MAX_NAME_LEN] = '\0';
    strncpy(hist->entries[idx].msg, msg, MAX_MSG_LEN);
    hist->entries[idx].msg[MAX_MSG_LEN] = '\0';

    pthread_mutex_unlock(&hist->lock);
}

void send_history(int client_fd, struct message_history *hist) {
    pthread_mutex_lock(&hist->lock);

    for (int i = 0; i < hist->count; i++) {
        int idx = (hist->start + i) % MAX_HISTORY;
        // Use sendNewMessage to send sender + message
        if (sendNewMessage(client_fd, hist->entries[idx].msg,
                           hist->entries[idx].sender) == -1) {
            // If sending fails, client probably disconnected – stop
            break;
        }
    }

    pthread_mutex_unlock(&hist->lock);
}

void destroy_history(struct message_history *hist) {
    pthread_mutex_destroy(&hist->lock);
}