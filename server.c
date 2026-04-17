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

// Δομή για να περάσουμε στο thread όλα τα δεδομένα που χρειάζεται.
// Θέλουμε και το client_fd για recvMessage,
// αλλά και το client_ip για όμορφη εκτύπωση του αποστολέα.
struct thread_args {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
};

// Αυτή είναι η thread function.
// Τρέχει σε ξεχωριστό νήμα και έχει μία μόνο δουλειά:
// να λαμβάνει μηνύματα από τον client και να τα εμφανίζει.
void *receive_messages(void *arg) {
    struct thread_args *args = (struct thread_args *)arg;

    int client_fd = args->client_fd;
    char *client_ip = args->client_ip;

    char msg[MAX_MSG_LEN + 1];
    char print_msg[MAX_PRINT_MSG_LEN + 1];

    while (1) {
        // Το thread περιμένει μήνυμα από τον client.
        // Εδώ επιτρέπεται να μπλοκάρει, γιατί είναι ξεχωριστό νήμα
        // και δεν σταματά το main thread από το να γράφει replies.
        ssize_t read_bytes = recvMessage(client_fd, msg, MAX_MSG_LEN);

        // read_bytes == 0 σημαίνει ότι ο client έκλεισε τη σύνδεση.
        if (read_bytes == 0) {
            snprintf(print_msg, sizeof(print_msg),
                     "\n-- User %s has left the conversation\n", client_ip);
            printMsg(stdout, print_msg);
            close(client_fd);
            exit(EXIT_SUCCESS);
        }
        // read_bytes == -1 σημαίνει σφάλμα κατά τη λήψη.
        else if (read_bytes == -1) {
            perror("recvMessage failure");
            close(client_fd);
            exit(EXIT_FAILURE);
        }
        // read_bytes == -2 σημαίνει ότι το μήνυμα ήταν μεγαλύτερο από το buffer.
        else if (read_bytes == -2) {
            printMsg(stderr, "\nerror: client message too large for buffer\n");
            close(client_fd);
            exit(EXIT_FAILURE);
        }

        // Εμφάνιση του μηνύματος που ήρθε από τον client.
        // Το "\nReply> " στο τέλος ξαναδείχνει το prompt,
        // ώστε το terminal να φαίνεται έτοιμο για νέο μήνυμα.
        snprintf(print_msg, sizeof(print_msg),
                 "\033[35mUser %s\033[0m> %s\nReply> ", client_ip, msg);
        printMsg(stdout, print_msg);
    }

    return NULL;
}

int main() {
    struct sockaddr_in addr;
    int socket_fd;
    int client_fd;
    int socket_option;

    // Χρησιμοποιούνται για να αποθηκεύσουμε τα στοιχεία του client
    // που συνδέεται στον server.
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];

    char reply[MAX_MSG_LEN + 1];
    pthread_t receiver_thread;

    // Τα arguments που θα περάσουμε στο receiver thread.
    struct thread_args args;

    // Δημιουργία TCP socket.
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket failure");
        exit(EXIT_FAILURE);
    }

    // Επιτρέπει γρηγορότερη επαναχρησιμοποίηση της ίδιας πόρτας
    // όταν ξανατρέχουμε τον server.
    socket_option = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                   &socket_option, sizeof(socket_option)) == -1) {
        close(socket_fd);
        perror("setsockopt failure");
        exit(EXIT_FAILURE);
    }

    // Ρυθμίσεις διεύθυνσης server.
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Συσχέτιση του socket με την τοπική διεύθυνση και πόρτα.
    if (bind(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        close(socket_fd);
        perror("bind failure");
        exit(EXIT_FAILURE);
    }

    // Το socket περνάει σε κατάσταση αναμονής για εισερχόμενες συνδέσεις.
    if (listen(socket_fd, SOMAXCONN) == -1) {
        close(socket_fd);
        perror("listen failure");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", PORT);

    // Αποδοχή μίας σύνδεσης από client.
    client_fd = accept(socket_fd, (struct sockaddr *) &client_addr, &client_addr_len);
    if (client_fd == -1) {
        close(socket_fd);
        perror("accept failure");
        exit(EXIT_FAILURE);
    }

    // Μετατροπή της IP του client από binary μορφή σε string.
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    // Μήνυμα ότι ο client συνδέθηκε.
    char print_msg[MAX_PRINT_MSG_LEN + 1];
    snprintf(print_msg, sizeof(print_msg),
             "-- User %s has joined the conversation\n", client_ip);
    printMsg(stdout, print_msg);

    // Γεμίζουμε τη δομή με τα στοιχεία που χρειάζεται το thread.
    args.client_fd = client_fd;
    strcpy(args.client_ip, client_ip);

    // Δημιουργία νέου thread που θα ακούει συνεχώς για μηνύματα από τον client.
    if (pthread_create(&receiver_thread, NULL, receive_messages, &args) != 0) {
        perror("pthread_create failure");
        close(client_fd);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    // Το main thread μένει υπεύθυνο για:
    // 1. διάβασμα από το πληκτρολόγιο
    // 2. αποστολή μηνυμάτων στον client
    while (1) {
        printMsg(stdout, "Reply> ");

        // Διαβάζουμε το μήνυμα που πληκτρολογεί ο server.
        if (fgets(reply, sizeof(reply), stdin) == NULL) {
            printMsg(stdout, "\nServer input closed\n");
            close(client_fd);
            close(socket_fd);
            break;
        }

        // Αφαιρούμε το '\n' που βάζει η fgets.
        reply[strcspn(reply, "\n")] = '\0';

        // Στέλνουμε το reply στον client.
        if (sendMessage(client_fd, reply) == -1) {
            perror("sendMessage failure");
            close(client_fd);
            close(socket_fd);
            exit(EXIT_FAILURE);
        }
    }

    // Περιμένουμε να τελειώσει το thread πριν τερματίσει τελείως ο server.
    pthread_join(receiver_thread, NULL);

    close(socket_fd);
    return 0;
}