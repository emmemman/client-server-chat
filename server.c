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

// Η λίστα με όλους τους συνδεδεμένους clients.
// Είναι global ώστε να μπορούν να τη χρησιμοποιούν όλα τα threads.
struct clientsList cl;

// Δομή ορισμάτων για κάθε thread.
// Κάθε thread πρέπει να ξέρει:
// 1. ποιο socket διαχειρίζεται
// 2. ποια είναι η IP του client (για εκτύπωση)
struct thread_args {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
};

// Thread function:
// Διαχειρίζεται έναν συγκεκριμένο πελάτη.
// Περιμένει μηνύματα από αυτόν και τα προωθεί στους υπόλοιπους.
void *handle_client(void *arg) {
    struct thread_args *args = (struct thread_args *)arg;

    int client_fd = args->client_fd;
    char client_ip[INET_ADDRSTRLEN];

    char msg[MAX_MSG_LEN + 1];
    char print_msg[MAX_PRINT_MSG_LEN + 1];
    char forward_msg[MAX_PRINT_MSG_LEN + 1];

    // Αντιγράφουμε την IP τοπικά, ώστε μετά να μπορούμε να ελευθερώσουμε το args
    strcpy(client_ip, args->client_ip);

    // Το arg είχε δεσμευτεί με malloc στη main, άρα τώρα δεν χρειάζεται άλλο
    free(args);

    while (1) {
        // Περιμένουμε μήνυμα από τον συγκεκριμένο client
        ssize_t read_bytes = recvMessage(client_fd, msg, MAX_MSG_LEN);

        // Ο client έκλεισε τη σύνδεση
        if (read_bytes == 0) {
            snprintf(print_msg, sizeof(print_msg),
                     "-- User %s has left the conversation\n", client_ip);
            printMsg(stdout, print_msg);

            // Αφαιρούμε τον client από τη λίστα
            removeClient(&cl, client_fd);

            close(client_fd);
            pthread_exit(NULL);
        }
        // Σφάλμα στη λήψη
        else if (read_bytes == -1) {
            perror("recvMessage failure");

            removeClient(&cl, client_fd);
            close(client_fd);
            pthread_exit(NULL);
        }
        // Πολύ μεγάλο μήνυμα
        else if (read_bytes == -2) {
            snprintf(print_msg, sizeof(print_msg),
                     "error: message from %s too large for buffer\n", client_ip);
            printMsg(stderr, print_msg);

            removeClient(&cl, client_fd);
            close(client_fd);
            pthread_exit(NULL);
        }

        // Εκτύπωση του μηνύματος στον server
        snprintf(print_msg, sizeof(print_msg),
                 "\033[35mUser %s\033[0m> %s\n", client_ip, msg);
        printMsg(stdout, print_msg);

        // Δημιουργούμε το μήνυμα που θα σταλεί στους άλλους clients
        snprintf(forward_msg, sizeof(forward_msg),
                 "User %s> %s", client_ip, msg);

        // Προώθηση του μηνύματος σε όλους τους συνδεδεμένους clients
        // Η clientsList είναι thread-safe, άρα μπορούμε να την
        // χρησιμοποιήσουμε με ασφάλεια από πολλά threads.
        for (int i = 0; i < getClientsCount(&cl); i++) {
            int fd = getClientAt(&cl, i);

            // Δεν ξαναστέλνουμε το μήνυμα στον αποστολέα
            if (fd != client_fd) {
                if (sendMessage(fd, forward_msg) == -1) {
                    perror("sendMessage failure");
                }
            }
        }
    }

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

    // Αρχικοποίηση της λίστας clients
    initClientsList(&cl);

    // Δημιουργία TCP socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket failure");
        destroyClientsList(&cl);
        exit(EXIT_FAILURE);
    }

    // Επιτρέπει γρήγορη επαναχρησιμοποίηση της πόρτας
    socket_option = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                   &socket_option, sizeof(socket_option)) == -1) {
        close(socket_fd);
        perror("setsockopt failure");
        destroyClientsList(&cl);
        exit(EXIT_FAILURE);
    }

    // Ρύθμιση διεύθυνσης server
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Συσχέτιση socket με IP και port
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(socket_fd);
        perror("bind failure");
        destroyClientsList(&cl);
        exit(EXIT_FAILURE);
    }

    // Ο server μπαίνει σε listening mode
    if (listen(socket_fd, SOMAXCONN) == -1) {
        close(socket_fd);
        perror("listen failure");
        destroyClientsList(&cl);
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", PORT);

    // Ο βασικός βρόχος της main κάνει μόνο accept νέων συνδέσεων
    while (1) {
        pthread_t tid;
        struct thread_args *args;
        char print_msg[MAX_PRINT_MSG_LEN + 1];

        client_addr_len = sizeof(client_addr);

        // Αναμονή για νέο client
        client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            perror("accept failure");
            continue;
        }

        // Μετατροπή IP σε string
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        // Εμφάνιση σύνδεσης νέου client
        snprintf(print_msg, sizeof(print_msg),
                 "-- User %s has joined the conversation\n", client_ip);
        printMsg(stdout, print_msg);

        // Προσθήκη του νέου client στη δομή clientsList
        addClient(&cl, client_fd);

        // Δεσμεύουμε μνήμη για τα ορίσματα του thread
        args = malloc(sizeof(struct thread_args));
        if (args == NULL) {
            perror("malloc failure");
            removeClient(&cl, client_fd);
            close(client_fd);
            continue;
        }

        args->client_fd = client_fd;
        strcpy(args->client_ip, client_ip);

        // Δημιουργούμε νέο thread για τον συγκεκριμένο client
        if (pthread_create(&tid, NULL, handle_client, args) != 0) {
            perror("pthread_create failure");
            removeClient(&cl, client_fd);
            close(client_fd);
            free(args);
            continue;
        }

        // Detached thread για να μη χρειαζόμαστε pthread_join
        pthread_detach(tid);
    }

    // Θεωρητικά δεν φτάνουμε εδώ λόγω while(1),
    // αλλά σωστά το αφήνουμε για πληρότητα.
    destroyClientsList(&cl);
    close(socket_fd);

    return 0;
}