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

//prédefinition
void* worker(void* arg);

//Nouvelle structure du client
typedef struct client_data_s {
    int sock;
    int client_id; 
    pthread_t thread;
    struct client_data_s* prev;
    struct client_data_s* next;
} client_data;

//Les variables locales
int nr_clients = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
client_data *first, *last;

//fonction clients
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
    free(cli);
    nr_clients--;
    pthread_mutex_unlock(&lock);
}

client_data* alloc_client(int sock) {
    pthread_mutex_lock(&lock);
    client_data* cli = (client_data*)malloc(sizeof(client_data));
    if (cli == NULL) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    cli->sock = sock;
    nr_clients++;
    cli->client_id = nr_clients;
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
    client_data* cli = alloc_client(client_socket);

    if (cli == NULL) {
        perror("Error in allocating client");
        close(client_socket);
        return;
    }

    printf("Nombre de clients actifs : %d\n", nr_clients);
    printf("Liste des clients connectés : ");
    client_data* current = first;
    while (current != NULL) {
        printf("%d ", current->sock);
        current = current->next;
    }
    printf("\n\n");

    pthread_create(&cli->thread, NULL, worker, (void*)cli);
}
//fin des fonctions clients

//les fonctions de commande
int do_echo(char *args, char *resp){
    snprintf(resp, 1024, "ok %s\n", args);
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
    strcpy(resp, "ok quitting\n");
    return -1;
}

int do_list(char *resp) {
    client_data* current = first;
    int client_id = nr_clients;
    resp[0] = '\0';
    while (current != NULL) {
        snprintf(resp + strlen(resp), 1024 - strlen(resp), "%d ", client_id++);
        current = current->next;
    }
    strcat(resp, "\n");
    return 0;
}
//fin des fonctions de commande

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

//fonctions gérant les arguments et commandes
int get_question(int sock, char *buffer, int *len, char* question) {
    int bytes_received;
    char ch;

    // On lit jusqu'à ce qu'on tombe sur un '\n'
    while ((bytes_received = read(sock, &ch, 1)) > 0) {
        if (ch == '\n') {
            break;
        }
        if (*len < 1024 - 1) {
            buffer[(*len)++] = ch;
        }
    }

    if (bytes_received < 0) {
        perror("Error in Read");
        return -1;
    }

    buffer[*len] = '\0'; // Terminate the string
    *len = 0; // Reset len for next use

    strcpy(question, buffer);

    return 0;
}

int eval_quest(client_data* cli, char *question, char *resp) {
    char *command, *args;
    char *saveptr;

    question[strcspn(question, "\r\n")] = '\0';
    command = strtok_r(question, " ", &saveptr);
    args = strtok_r(NULL, "", &saveptr);

    if (strcmp(command, "echo") == 0){
        return do_echo(args, resp);
    }
    else if (strcmp(command, "random") == 0){
        return do_rand(args, resp);
    }
    else if (strcmp(command, "time") == 0){
        return do_time(resp);
    }
    else if (strcmp(command, "quit") == 0){
        return do_quit(cli, resp);
    } 
    else if (strcmp(command, "list") == 0){
        return do_list(resp);
    }
    else {
        snprintf(resp, 1024, "fail unknown command '%s'\n", command);
        return 0;
    }
}
//fin des fonctions gérant les arguments et commandes

void* worker(void* arg) {
    client_data* cli = (client_data*)arg;
    char buf[1024];
    char question[1024];
    char response[1024];
    int len = 0;

    while (1) {
        if (get_question(cli->sock, buf, &len, question) < 0)
            break; /* erreur: on déconnecte le client */
        if (eval_quest(cli, question, response) < 0)
            break; /* erreur: on déconnecte le client */
        if (write_full(cli->sock, response, strlen(response)) < 0) {
            perror("could not send message");
            break; /* erreur: on déconnecte le client */
        }
        if (strcmp(response, "ok quitting\n") == 0)
            break; // quit the loop when client quits
    }

    free_client(cli);
    printf("******* \t Un Client s'est déconnecté. \t *******\n");
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
