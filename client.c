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

void *receive_messages(void *arg) {
    int socket_fd = *(int *)arg;
    char reply[MAX_MSG_LEN + 1];
    char sender[MAX_NAME_LEN + 1];
    char print_msg[MAX_PRINT_MSG_LEN + 1];

    while (1) {
        //receive message with sender from server
        ssize_t read_bytes = recvNewMessage(socket_fd, reply, MAX_MSG_LEN,
                                            sender, MAX_NAME_LEN);

        if (read_bytes == 0) {
            printMsg(stdout, "\nServer disconnected\n");
            shutdown(socket_fd, SHUT_RDWR);
            break;
        } else if (read_bytes == -1) {
            perror("recvNewMessage failure");
            shutdown(socket_fd, SHUT_RDWR);
            break;
        } else if (read_bytes == -2) {
            printMsg(stderr, "\nerror: server message or sender too large for buffer\n");
            shutdown(socket_fd, SHUT_RDWR);
            break;
        }

        //emfanizw to mhnuma me to onoma tou sender
        snprintf(print_msg, sizeof(print_msg), "%s> %s\n> ", sender, reply);
        printMsg(stdout, print_msg);
    }

    return NULL;
}

int main() {
    struct sockaddr_in addr;
    int socket_fd;
    char msg[MAX_MSG_LEN + 1];
    pthread_t receiver_thread;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket failure");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP_ADDRESS, &addr.sin_addr) <= 0) {
        close(socket_fd);
        perror("inet_pton failure");
        exit(EXIT_FAILURE);
    }

    if (connect(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        close(socket_fd);
        perror("connect failure");
        exit(EXIT_FAILURE);
    }

    printMsg(stdout, "Connected to server\n");

    //thread gia mhnumata apo server
    if (pthread_create(&receiver_thread, NULL, receive_messages, &socket_fd) != 0) {
        perror("pthread_create failure");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        printMsg(stdout, "> ");

        if (fgets(msg, sizeof(msg), stdin) == NULL) {
            printMsg(stdout, "\nDisconnected from server\n");
            shutdown(socket_fd, SHUT_RDWR);
            break;
        }

        msg[strcspn(msg, "\n")] = '\0';

        if (sendMessage(socket_fd, msg) == -1) {
            perror("sendMessage failure");
            shutdown(socket_fd, SHUT_RDWR);
            break;
        }
    }

    pthread_join(receiver_thread, NULL);
    close(socket_fd);
    return 0;
}