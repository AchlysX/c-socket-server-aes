#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <pthread.h>

//Les variables locales
int nr_clients = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

//Structure du client
typedef struct client_data_s {
    int sock;
    int client_id; // Pour faire plus organisé, on a rajouté un id client afin de reconnaitre qui envoie tel et tel message.
    pthread_t thread;
} client_data;

//fonctions clients
void init_clients() {
    nr_clients = 0;
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
    pthread_mutex_unlock(&lock);
    return cli;
}

void free_client(client_data* cli) {
    pthread_mutex_lock(&lock);
    close(cli->sock);
    free(cli);
    nr_clients--;
    pthread_mutex_unlock(&lock);
}
//fin des fonctions clients

void* worker(void* arg) {
    client_data* cli = (client_data*)arg;
    char message_reçu[1024];
    char reponse[2048];
    int bytes_received;

    while ((bytes_received = read(cli->sock, message_reçu, sizeof(message_reçu) - 1)) > 0) {
        message_reçu[bytes_received] = '\0';

        printf("Client %d: %i octets reçus\n", cli->client_id, bytes_received);
        printf("Client %d: Message reçu:\t %s\n\n", cli->client_id, message_reçu);

        snprintf(reponse, sizeof(reponse) - 1, "Message envoyé : %s\n", message_reçu);

        if (write(cli->sock, reponse, strlen(reponse)) < 0) {
            perror("Error in write");
            break;
        }
    }

    if (bytes_received < 0) {
        perror("Error in Read");
    }

    free_client(cli);
    printf("******* \t Un Client s'est déconnecté. \t *******\n");
    free_client(cli);
    printf("Clients réstants: %d \n \n",nr_clients);

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
    int port_number = 4242;

    if (argc == 2) {
        port_number = atoi(argv[1]);
    }

    init_clients();

    listen_port(port_number);

    return 0;
}
