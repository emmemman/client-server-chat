#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "config.h"
#include "readwrite.h"
#include "msg.h"
#include "printMsg.h"
#include "clientsList.h"
#include "history.h"

//clients list
struct clientsList cl;
struct message_history history;

//mutex gia onomata
static pthread_mutex_t name_counter_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned int name_counter = 0;

struct thread_args {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
};

void *handle_client(void *arg) {
    struct thread_args *args = (struct thread_args *)arg;
    int client_fd = args->client_fd;
    char client_ip[INET_ADDRSTRLEN];
    strcpy(client_ip, args->client_ip);
    free(args); 

    //dinw onoma
    char sender_name[MAX_NAME_LEN];
    pthread_mutex_lock(&name_counter_lock);
    snprintf(sender_name, sizeof(sender_name), "User%u", ++name_counter);
    pthread_mutex_unlock(&name_counter_lock);

    char msg[MAX_MSG_LEN + 1];
    char print_msg[MAX_PRINT_MSG_LEN + 1];

    snprintf(print_msg, sizeof(print_msg),
             "-- %s (%s) has joined the conversation\n", sender_name, client_ip);
    printMsg(stdout, print_msg);

    //stelnw history
    send_history(client_fd, &history);

    while (1) {
        //receive mhnuma
        ssize_t read_bytes = recvMessage(client_fd, msg, sizeof(msg) - 1);

        if (read_bytes == 0) {
            snprintf(print_msg, sizeof(print_msg),
                     "-- %s (%s) has left the conversation\n", sender_name, client_ip);
            printMsg(stdout, print_msg);
            break;
        }
        else if (read_bytes == -1) {
            perror("recvMessage failure");
            break;
        }
        else if (read_bytes == -2 || read_bytes == -3) {
            snprintf(print_msg, sizeof(print_msg),
                     "error: message from %s too large\n", sender_name);
            printMsg(stderr, print_msg);
            break;
        }

        msg[read_bytes] = '\0';

        //elegxos gia allagh onomatos
        if (strncmp(msg, "\\name ", 6) == 0) {
            //kainourgio onoma
            char *new_name = msg + 6;

            size_t len = strlen(new_name);
            while (len > 0 && (new_name[len-1] == '\n' || new_name[len-1] == '\r'))
                new_name[--len] = '\0';

            if (len > 0 && len < MAX_NAME_LEN) {
                strcpy(sender_name, new_name);
                snprintf(print_msg, sizeof(print_msg),
                         "-- %s changed name to %s\n", client_ip, sender_name);
                printMsg(stdout, print_msg);
            } else {
                snprintf(print_msg, sizeof(print_msg),
                         "error: invalid name length\n");
                printMsg(stderr, print_msg);
            }
            continue;
        }

        //typwnw mhnuma
        snprintf(print_msg, sizeof(print_msg),
                 "\033[35m%s\033[0m> %s\n", sender_name, msg);
        printMsg(stdout, print_msg);

        add_history(&history, sender_name, msg);

        broadcastClientsList(&cl, msg, sender_name, client_fd);
    }

    removeClient(&cl, client_fd);
    close(client_fd);
    return NULL;
}

int main() {
    struct sockaddr_in addr;
    int socket_fd;
    int socket_option;

    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];

    initClientsList(&cl);

    init_history(&history);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket failure");
        destroyClientsList(&cl);
        exit(EXIT_FAILURE);
    }

    socket_option = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                   &socket_option, sizeof(socket_option)) == -1) {
        close(socket_fd);
        perror("setsockopt failure");
        destroyClientsList(&cl);
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(socket_fd);
        perror("bind failure");
        destroyClientsList(&cl);
        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, SOMAXCONN) == -1) {
        close(socket_fd);
        perror("listen failure");
        destroyClientsList(&cl);
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", PORT);

    while (1) {
        pthread_t tid;
        struct thread_args *args;
        char print_msg[MAX_PRINT_MSG_LEN + 1];

        client_addr_len = sizeof(client_addr);
        client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            perror("accept failure");
            continue;
        }

        // metatroph client IP to string
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        if (addClient(&cl, client_fd) == -1) {
            snprintf(print_msg, sizeof(print_msg),
                     "error: maximum clients reached, rejecting %s\n", client_ip);
            printMsg(stderr, print_msg);
            close(client_fd);
            continue;
        }

        args = malloc(sizeof(struct thread_args));
        if (args == NULL) {
            perror("malloc failure");
            removeClient(&cl, client_fd);
            close(client_fd);
            continue;
        }
        args->client_fd = client_fd;
        strcpy(args->client_ip, client_ip);

        if (pthread_create(&tid, NULL, handle_client, args) != 0) {
            perror("pthread_create failure");
            removeClient(&cl, client_fd);
            close(client_fd);
            free(args);
            continue;
        }
        pthread_detach(tid);
    }


    destroy_history(&history);
    destroyClientsList(&cl);
    close(socket_fd);
    return 0;
}