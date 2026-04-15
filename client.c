#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "readwrite.h"
#include "msg.h"
#include "printMsg.h"

int main() {
    struct sockaddr_in addr;
    int socket_fd;
    char msg[MAX_MSG_LEN + 1];
    char reply[MAX_MSG_LEN + 1];
    char print_msg[MAX_PRINT_MSG_LEN + 1];

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

    while (1) {
        printMsg(stdout, "> ");

		//diavazw apo plhktrologio
        if (fgets(msg, sizeof(msg), stdin) == NULL) {
            printMsg(stdout, "\nDisconnected from server\n");
            close(socket_fd);
            break;
        }

        msg[strcspn(msg, "\n")] = '\0';

		//stelnw mhmuma
        if (sendMessage(socket_fd, msg) == -1) {
            perror("sendMessage failure");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

		//stamataw mexri na apanthsei o server
        ssize_t read_bytes = recvMessage(socket_fd, reply, MAX_MSG_LEN);

        if (read_bytes == 0) {
            printMsg(stdout, "Server disconnected\n");
            close(socket_fd);
            break;
        } else if (read_bytes == -1) {
            perror("recvMessage failure");
            close(socket_fd);
            exit(EXIT_FAILURE);
        } else if (read_bytes == -2) {
            printMsg(stderr, "error: server message too large for buffer\n");
            close(socket_fd);
            exit(EXIT_FAILURE);
        }

        snprintf(print_msg, sizeof(print_msg), "Server> %s\n", reply);
        printMsg(stdout, print_msg);
    }

    return 0;
}