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

#define MAX_CLIENTS 100

// =========================
// ΔΟΜΗ CLIENT
// =========================
// Κρατάμε για κάθε συνδεδεμένο client:
// 1. το socket descriptor του
// 2. την IP του σε μορφή string
struct client_info {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
};

// =========================
// ΠΑΓΚΟΣΜΙΑ ΛΙΣΤΑ CLIENTS
// =========================
// Ο server πρέπει να ξέρει ποιοι clients είναι συνδεδεμένοι
// ώστε όταν λάβει μήνυμα από έναν client να το στείλει στους άλλους.
struct client_info clients[MAX_CLIENTS];
int client_count = 0;

// Mutex για συγχρονισμό πρόσβασης στη λίστα clients.
// Είναι απαραίτητο γιατί πολλά threads θα διαβάζουν/αλλάζουν
// ταυτόχρονα τη δομή clients[].
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// =========================
// ΔΟΜΗ ARGUMENTS ΓΙΑ THREAD
// =========================
// Όταν φτιάχνουμε ένα thread για έναν client,
// του περνάμε όλα τα στοιχεία που χρειάζεται.
struct thread_args {
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
};

// =========================
// ΠΡΟΣΘΗΚΗ CLIENT ΣΤΗ ΛΙΣΤΑ
// =========================
void add_client(int client_fd, const char *client_ip) {
    pthread_mutex_lock(&clients_mutex);

    if (client_count < MAX_CLIENTS) {
        clients[client_count].client_fd = client_fd;
        strncpy(clients[client_count].client_ip, client_ip, INET_ADDRSTRLEN - 1);
        clients[client_count].client_ip[INET_ADDRSTRLEN - 1] = '\0';
        client_count++;
    }

    pthread_mutex_unlock(&clients_mutex);
}

// =========================
// ΑΦΑΙΡΕΣΗ CLIENT ΑΠΟ ΤΗ ΛΙΣΤΑ
// =========================
// Όταν ένας client αποσυνδεθεί, πρέπει να φύγει από τη λίστα.
// Για να μη μείνουν "τρύπες", μεταφέρουμε τον τελευταίο client
// στη θέση αυτού που φεύγει.
void remove_client(int client_fd) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_fd == client_fd) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// =========================
// BROADCAST ΜΗΝΥΜΑΤΟΣ
// =========================
// Στέλνουμε το μήνυμα σε όλους τους clients
// εκτός από αυτόν που το έστειλε.
void broadcast_message(const char *message, int sender_fd) {
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++) {
        if (clients[i].client_fd != sender_fd) {
            if (sendMessage(clients[i].client_fd, message) == -1) {
                perror("sendMessage failure");
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

// =========================
// THREAD FUNCTION ΓΙΑ ΚΑΘΕ CLIENT
// =========================
// Κάθε client έχει το δικό του thread.
// Το thread:
// 1. περιμένει μηνύματα από τον client
// 2. όταν λάβει μήνυμα, το εμφανίζει στον server
// 3. το προωθεί στους υπόλοιπους clients
// 4. αν ο client φύγει, τον αφαιρεί από τη λίστα
void *handle_client(void *arg) {
    struct thread_args *args = (struct thread_args *)arg;

    int client_fd = args->client_fd;
    char client_ip[INET_ADDRSTRLEN];
    char msg[MAX_MSG_LEN + 1];
    char print_msg[MAX_PRINT_MSG_LEN + 1];
    char broadcast_msg[MAX_PRINT_MSG_LEN + 1];

    // Κρατάμε το IP τοπικά, γιατί μετά θα ελευθερώσουμε το arg
    strncpy(client_ip, args->client_ip, INET_ADDRSTRLEN - 1);
    client_ip[INET_ADDRSTRLEN - 1] = '\0';

    // Δεν χρειαζόμαστε πια τη δεσμευμένη μνήμη των arguments
    free(arg);

    while (1) {
        // Περιμένουμε μήνυμα από τον συγκεκριμένο client
        ssize_t read_bytes = recvMessage(client_fd, msg, MAX_MSG_LEN);

        // Ο client έκλεισε κανονικά τη σύνδεση
        if (read_bytes == 0) {
            snprintf(print_msg, sizeof(print_msg),
                     "-- User %s has left the conversation\n", client_ip);
            printMsg(stdout, print_msg);

            remove_client(client_fd);
            close(client_fd);
            pthread_exit(NULL);
        }
        // Σφάλμα στη λήψη
        else if (read_bytes == -1) {
            perror("recvMessage failure");

            remove_client(client_fd);
            close(client_fd);
            pthread_exit(NULL);
        }
        // Το μήνυμα ήταν μεγαλύτερο από το επιτρεπτό buffer
        else if (read_bytes == -2) {
            snprintf(print_msg, sizeof(print_msg),
                     "error: message from %s too large for buffer\n", client_ip);
            printMsg(stderr, print_msg);

            remove_client(client_fd);
            close(client_fd);
            pthread_exit(NULL);
        }

        // Εμφάνιση του μηνύματος στον server
        snprintf(print_msg, sizeof(print_msg),
                 "\033[35mUser %s\033[0m> %s\n", client_ip, msg);
        printMsg(stdout, print_msg);

        // Δημιουργία μορφοποιημένου μηνύματος για αποστολή στους άλλους clients
        snprintf(broadcast_msg, sizeof(broadcast_msg),
                 "User %s> %s", client_ip, msg);

        // Αποστολή του μηνύματος σε όλους τους υπόλοιπους
        broadcast_message(broadcast_msg, client_fd);
    }

    return NULL;
}

// =========================
// MAIN
// =========================
int main() {
    struct sockaddr_in addr;
    int socket_fd;
    int socket_option;

    // Στοιχεία του client που συνδέεται κάθε φορά
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];

    // Δημιουργία TCP socket
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket failure");
        exit(EXIT_FAILURE);
    }

    // Επιτρέπει επαναχρησιμοποίηση της θύρας
    socket_option = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                   &socket_option, sizeof(socket_option)) == -1) {
        close(socket_fd);
        perror("setsockopt failure");
        exit(EXIT_FAILURE);
    }

    // Ρύθμιση διεύθυνσης server
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Συσχέτιση socket με IP/port
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(socket_fd);
        perror("bind failure");
        exit(EXIT_FAILURE);
    }

    // Το socket μπαίνει σε listening mode
    if (listen(socket_fd, SOMAXCONN) == -1) {
        close(socket_fd);
        perror("listen failure");
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d\n", PORT);

    // =========================
    // ΒΑΣΙΚΟΣ ΒΡΟΧΟΣ SERVER
    // =========================
    // Εδώ είναι η μεγάλη αλλαγή της Έκδοσης #3:
    // Το main ΔΕΝ διαβάζει από πληκτρολόγιο.
    // Το μόνο που κάνει είναι να δέχεται συνεχώς νέες συνδέσεις.
    while (1) {
        pthread_t tid;
        struct thread_args *args;

        client_addr_len = sizeof(client_addr);

        // Περιμένουμε νέο client
        client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            perror("accept failure");
            continue;
        }

        // Μετατροπή της IP του client σε string
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

        // Εμφάνιση μηνύματος σύνδεσης
        char print_msg[MAX_PRINT_MSG_LEN + 1];
        snprintf(print_msg, sizeof(print_msg),
                 "-- User %s has joined the conversation\n", client_ip);
        printMsg(stdout, print_msg);

        // Προσθήκη του client στη λίστα των ενεργών clients
        add_client(client_fd, client_ip);

        // Δεσμεύουμε μνήμη για τα arguments του thread
        args = malloc(sizeof(struct thread_args));
        if (args == NULL) {
            perror("malloc failure");
            remove_client(client_fd);
            close(client_fd);
            continue;
        }

        args->client_fd = client_fd;
        strncpy(args->client_ip, client_ip, INET_ADDRSTRLEN - 1);
        args->client_ip[INET_ADDRSTRLEN - 1] = '\0';

        // Δημιουργία thread για διαχείριση του συγκεκριμένου client
        if (pthread_create(&tid, NULL, handle_client, args) != 0) {
            perror("pthread_create failure");
            remove_client(client_fd);
            close(client_fd);
            free(args);
            continue;
        }

        // Detach ώστε να μην χρειάζεται pthread_join για κάθε client
        pthread_detach(tid);
    }

    close(socket_fd);
    return 0;
}