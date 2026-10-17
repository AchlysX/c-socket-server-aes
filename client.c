#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFFER_SIZE 1024

// Connexion au serveur via TCP
int connect_server(const char *host, const char *port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // Parcourir les résultats et se connecter au premier possible
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        freeaddrinfo(servinfo);
        return -1;
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

// Envoi d'un message complet au serveur
int write_all(int sockfd, const char *buf, size_t len) {
    size_t total_sent = 0;
    ssize_t sent_bytes;

    while (total_sent < len) {
        sent_bytes = send(sockfd, buf + total_sent, len - total_sent, 0);
        if (sent_bytes == -1) {
            perror("send");
            return -1;
        }
        total_sent += sent_bytes;
    }

    return 0;
}

// Réception de la réponse du serveur jusqu'au saut de ligne
int get_response(int sockfd, char *buf, size_t buf_size) {
    ssize_t bytes_received;
    size_t total_bytes = 0;

    while (total_bytes < buf_size - 1) {
        bytes_received = recv(sockfd, buf + total_bytes, 1, 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Server closed connection.\n");
            } else {
                perror("recv");
            }
            return -1;
        }

        total_bytes += bytes_received;
        if (buf[total_bytes - 1] == '\n') {
            break;
        }
    }

    buf[total_bytes] = '\0';
    return 0;
}

// Boucle principale de communication non chiffrée (v1 à v3)
void speak_to_server(int sockfd) {
    char input[BUFFER_SIZE];
    char response[BUFFER_SIZE];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
        input[strcspn(input, "\r\n")] = '\0'; 
        
        if (strlen(input) == 0) continue;

        // Envoi de la commande avec saut de ligne
        if (write_all(sockfd, input, strlen(input)) == -1) break;
        if (write_all(sockfd, "\n", 1) == -1) break;

        // Réception de la réponse
        if (get_response(sockfd, response, sizeof(response)) == -1) break;
        printf("Server: %s", response);
        if (response[strlen(response) - 1] != '\n') {
            printf("\n");
        }

        if (strcmp(input, "quit") == 0 || strcmp(input, "/quit") == 0 || strcmp(input, "q") == 0) {
            break;
        }
    }
    close(sockfd);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s host port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *host = argv[1];
    const char *port = argv[2];

    int sockfd = connect_server(host, port);
    if (sockfd == -1) {
        exit(EXIT_FAILURE);
    }

    speak_to_server(sockfd);

    return 0;
}