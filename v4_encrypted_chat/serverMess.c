#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include "crypto.h"

// Structure des messages
typedef struct msg {
    char sender[9];
    char* text;
    struct msg * next;
} msg;

// Structure de la boite aux lettres
typedef struct mbox {
    msg *first, *last;
} mbox;

// Structure du client
typedef struct client_data_s {
    int sock;
    int client_id; 
    pthread_t thread;
    struct client_data_s* prev;
    struct client_data_s* next;
    char alias[9];
    mbox box; 
    pthread_mutex_t box_lock;
} client_data;

// Prédefinitions
void* worker(void* arg);
client_data* search_client(char* alias);

// Variables globales
int nr_clients = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
client_data *first = NULL, *last = NULL;

// Fonctions agissant sur les messages
msg* create_msg(char* author, char* contents) {
    msg* message = malloc(sizeof(msg));
    if (!message) return NULL;
    message->text = strdup(contents);
    strncpy(message->sender, author, 8);
    message->sender[8] = '\0';
    message->next = NULL;
    return message;
}

void destroy_msg(msg* mess) {
    if (mess) {
        free(mess->text);
        free(mess);
    }
}

// Fonctions agissant sur la mailbox
void put(client_data* cli, msg* mess) {
    pthread_mutex_lock(&cli->box_lock);
    if (cli->box.first == NULL) {
        cli->box.first = mess;
        cli->box.last = mess;
    } else {
        cli->box.last->next = mess;
        cli->box.last = mess;
    }
    pthread_mutex_unlock(&cli->box_lock);
}

msg* get(client_data* cli) {
    pthread_mutex_lock(&cli->box_lock);
    if (cli->box.first == NULL) {
        pthread_mutex_unlock(&cli->box_lock);
        return NULL;
    }
    msg* temp = cli->box.first;
    cli->box.first = temp->next;
    if (cli->box.first == NULL) {
        cli->box.last = NULL;
    }
    pthread_mutex_unlock(&cli->box_lock);
    temp->next = NULL;
    return temp;
}

// Fonctions clients
void init_clients() {
    nr_clients = 0;
    first = NULL;
    last = NULL;
}

void free_client(client_data* cli) {
    pthread_mutex_lock(&lock);
    if (cli->prev != NULL) {
        cli->prev->next = cli->next;
    } else {
        first = cli->next;
    }
    if (cli->next != NULL) {
        cli->next->prev = cli->prev;
    } else {
        last = cli->prev;
    }
    close(cli->sock);

    // Vider les messages restants dans la boîte
    msg* m = cli->box.first;
    while (m != NULL) {
        msg* next = m->next;
        destroy_msg(m);
        m = next;
    }
    pthread_mutex_destroy(&cli->box_lock);
    free(cli);
    nr_clients--;
    pthread_mutex_unlock(&lock);
}

client_data* alloc_client(int sock) {
    pthread_mutex_lock(&lock);
    client_data* cli = (client_data*)calloc(1, sizeof(client_data));
    if (cli == NULL) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    cli->sock = sock;
    cli->client_id = ++nr_clients;
    pthread_mutex_init(&cli->box_lock, NULL);
    cli->prev = last;
    cli->next = NULL;
    if (last != NULL) {
        last->next = cli;
    }
    last = cli;
    if (first == NULL) {
        first = cli;
    }
    pthread_mutex_unlock(&lock);
    return cli;
}

void client_arrived(int client_socket) {
    printf("Client arrived with socket %d\n", client_socket);
    client_data* cli = alloc_client(client_socket);

    if (cli == NULL) {
        perror("Error in allocating client");
        close(client_socket);
        return;
    }

    printf("Client %d allocated\n", cli->client_id);
    printf("Nombre de clients actifs : %d\n", nr_clients);

    pthread_create(&cli->thread, NULL, worker, (void*)cli);
    pthread_detach(cli->thread);
}   

int valid_alias(char* alias) {
    if (!alias) return 0;
    size_t len = strlen(alias);
    if (len < 1 || len > 8) return 0;

    for (size_t i = 0; i < len; i++) {
        if (!isalnum(alias[i])) return 0;
    }
    return 1;
}

// Fonctions de commande
int do_echo(char *args, char *resp) {
    snprintf(resp, 1024, "ok %s\n", args ? args : "");
    return 0;
}

int do_rand(char *args, char *resp) {
    int rand_num;
    if (args == NULL) {
        rand_num = rand();
    } else {
        int max_val = atoi(args);
        if (max_val <= 0) {
            strcpy(resp, "fail invalid argument for random\n");
            return 0;
        }
        rand_num = rand() % max_val;
    }
    snprintf(resp, 1024, "ok %d\n", rand_num);
    return 0;
}

int do_time(char *resp) {
    time_t current_time;
    struct tm *local_time;

    time(&current_time);
    local_time = localtime(&current_time);

    snprintf(resp, 1024, "ok %s", asctime(local_time));
    return 0;
}

int do_quit(client_data* cli, char *resp) {
    (void)cli;
    strcpy(resp, "ok quitting\n");
    return -1; // Indique au worker thread de terminer la session
}

int do_list(char *resp) {
    pthread_mutex_lock(&lock);
    client_data* current = first;
    resp[0] = '\0'; 

    while (current != NULL) {
        if (strlen(current->alias) > 0) {
            snprintf(resp + strlen(resp), 1024 - strlen(resp), "%s ", current->alias);
        }
        current = current->next;
    }
    pthread_mutex_unlock(&lock);

    strcat(resp, "\n");
    return 0;
}

int do_alias(client_data* cli, char *args, char *resp) {
    if (args == NULL || !valid_alias(args)) {
        strcpy(resp, "fail invalid alias\n");
        return 0;
    }

    pthread_mutex_lock(&lock);
    if (strlen(cli->alias) > 0) {
        pthread_mutex_unlock(&lock);
        strcpy(resp, "fail alias already set\n");
        return 0;
    }

    client_data* current = first;
    while (current != NULL) {
        if (strcasecmp(current->alias, args) == 0) {
            pthread_mutex_unlock(&lock);
            strcpy(resp, "fail alias already exists\n");
            return 0;
        }
        current = current->next;
    }

    strncpy(cli->alias, args, 8);
    cli->alias[8] = '\0';
    pthread_mutex_unlock(&lock);

    strcpy(resp, "ok\n");
    return 0;
}

client_data* search_client(char* alias) {
    pthread_mutex_lock(&lock);
    client_data* current = first;
    while (current != NULL) {
        if (strlen(current->alias) > 0 && strcmp(current->alias, alias) == 0) {
            pthread_mutex_unlock(&lock);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

int do_send(client_data* cli, char *args, char *resp) {
    if (strlen(cli->alias) == 0) {
        strcpy(resp, "fail anonymous user\n");
        return 0;
    }
    if (args == NULL) {
        strcpy(resp, "fail invalid arguments\n");
        return 0;
    }

    char *saveptr;
    char *recipient = strtok_r(args, " ", &saveptr);
    char *message = strtok_r(NULL, "", &saveptr);

    if (recipient == NULL || message == NULL) {
        strcpy(resp, "fail invalid arguments\n");
        return 0;
    }

    client_data* recipient_cli = search_client(recipient);
    if (recipient_cli == NULL) {
        strcpy(resp, "fail recipient not found\n");
        return 0;
    }

    msg* sent = create_msg(cli->alias, message);
    if (sent != NULL) {
        put(recipient_cli, sent);
    }
    strcpy(resp, "ok\n");
    return 0;
}

int do_recv(client_data* cli, char *resp) {
    if (strlen(cli->alias) == 0) {
        strcpy(resp, "fail anonymous user\n");
        return 0;
    }

    msg* message = get(cli);
    if (message == NULL) {
        strcpy(resp, "ok\n"); // Pas de message en attente
        return 0;
    }

    snprintf(resp, 1024, "ok %s %s\n", message->sender, message->text);
    destroy_msg(message);
    return 0;
}

int write_full(int sock, const void *buf, size_t len) {
    ssize_t sent_bytes;
    size_t total_sent = 0;

    while (total_sent < len) {
        sent_bytes = send(sock, buf + total_sent, len - total_sent, 0);
        if (sent_bytes == -1) {
            perror("send");
            return -1;
        }
        total_sent += sent_bytes;
    }

    return 0;
}

int get_question(int sock, char *buffer, int *len, char* question) {
    int bytes_received;
    char ch;

    while ((bytes_received = read(sock, &ch, 1)) > 0) {
        if (ch == '\n') {
            break;
        }
        if (*len < 2048 - 1) {
            buffer[(*len)++] = ch;
        }
    }

    if (bytes_received <= 0 && *len == 0) {
        return -1;
    }

    buffer[*len] = '\0';
    *len = 0;

    strcpy(question, buffer);
    return 0;
}

int eval_quest(client_data* cli, char *question, char *resp) {
    char *command, *args;
    char *saveptr;

    question[strcspn(question, "\r\n")] = '\0';
    command = strtok_r(question, " ", &saveptr);
    if (command == NULL) {
        resp[0] = '\0';
        return 0;
    }

    // Permet d'accepter les commandes avec ou sans '/'
    if (command[0] == '/') {
        command++;
    }

    args = strtok_r(NULL, "", &saveptr);

    if (strcmp(command, "echo") == 0) {
        return do_echo(args, resp);
    } else if (strcmp(command, "random") == 0) {
        return do_rand(args, resp);
    } else if (strcmp(command, "time") == 0) {
        return do_time(resp);
    } else if (strcmp(command, "quit") == 0) {
        return do_quit(cli, resp);
    } else if (strcmp(command, "list") == 0) {
        return do_list(resp);
    } else if (strcmp(command, "alias") == 0) {
        return do_alias(cli, args, resp);
    } else if (strcmp(command, "send") == 0) {
        return do_send(cli, args, resp);
    } else if (strcmp(command, "recv") == 0) {
        return do_recv(cli, resp);
    } else {
        snprintf(resp, 1024, "fail unknown command '%s'\n", command);
        return 0;
    }
}

void* worker(void* arg) {
    client_data* cli = (client_data*)arg;
    char buf[2048];
    char encrypted_in[2048];
    char decrypted_in[MAX_B64];
    char plaintext_resp[1024];
    char encrypted_out[2048];
    int len = 0;

    while (1) {
        if (get_question(cli->sock, buf, &len, encrypted_in) < 0) {
            break; // Client déconnecté ou erreur
        }

        // Décryptage du message entrant
        if (decrypt_msg(encrypted_in, decrypted_in, sizeof(decrypted_in)) <= 0) {
            snprintf(plaintext_resp, sizeof(plaintext_resp), "fail decryption error\n");
            encrypt_msg(plaintext_resp, encrypted_out, sizeof(encrypted_out));
            write_full(cli->sock, encrypted_out, strlen(encrypted_out));
            continue;
        }

        int ret = eval_quest(cli, decrypted_in, plaintext_resp);

        // Chiffrement et envoi de la réponse
        if (strlen(plaintext_resp) > 0) {
            encrypt_msg(plaintext_resp, encrypted_out, sizeof(encrypted_out));
            if (write_full(cli->sock, encrypted_out, strlen(encrypted_out)) < 0) {
                break;
            }
        }

        if (ret < 0) {
            break; // Commande quit demandée
        }
    }

    free_client(cli);

    printf("*** \t Un Client s'est déconnecté. \t ***\n");
    printf("Clients restants: %d \n \n", nr_clients);

    pthread_exit(NULL);
}

void listen_port(int port_number) {
    int server_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in6 server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin6_family = AF_INET6;
    server_address.sin6_port = htons(port_number);
    server_address.sin6_addr = in6addr_any;

    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        exit(1);
    }

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind");
        close(server_socket);
        exit(1);
    }

    if (listen(server_socket, SOMAXCONN) < 0) {
        perror("listen");
        close(server_socket);
        exit(1);
    }
    printf("***** \t En attente de clients ... \t*****\n\n");

    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        printf("\tConnexion établie! - client arrivé - \n\n");
        client_arrived(client_socket);
    }
    close(server_socket);
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));

    int port_number = 4242;

    if (argc == 2) {
        port_number = atoi(argv[1]);
    }

    init_clients();

    listen_port(port_number);

    return 0;
}