#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>


void client_arrived(int client_socket) {
    char message_reçu[1024];
    char reponse[2048];

    int bytes_received;
    while ((bytes_received = read(client_socket, message_reçu, sizeof(message_reçu) - 1)) > 0) {
        message_reçu[bytes_received] = '\0';

        printf("%i bytes received\n", bytes_received);
        printf("Received message:\t %s\n\n", message_reçu);

        snprintf(reponse, sizeof(reponse) - 1, "Message sent : %s\n", message_reçu);

        if (write(client_socket, reponse, strlen(reponse)) < 0) {
            perror("Error in write");
            break;
        }
    }

    if (bytes_received < 0) {
        perror("Error in Read");
    }

    close(client_socket);
    printf("******* \t Client disconnected. \t *******\n");
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
    printf("***** \t Awaiting clients... \t*****\n\n");

    while (1) {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        printf("\tConnexion established - client has arrived - \n\n");
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

    listen_port(port_number);

    return 0;
}
