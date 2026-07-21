#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")

    typedef int socklen_t;
    #define close closesocket

    int set_nonblocking(int sock) {
        unsigned long mode = 1;
        return ioctlsocket(sock, FIONBIO, &mode);
    }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>

    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1

    int set_nonblocking(int sock) {
        int flags = fcntl(sock, F_GETFL, 0);
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
#endif

#include "../include/network.h"

#ifdef _WIN32
void init_winsock() {
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
}
#else
void init_winsock() {
    /* Su Linux non serve */
}
#endif

int create_server_socket(int port) {
    init_winsock();

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        printf("Errore: impossibile creare socket\n");
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printf("Errore: setsockopt fallito\n");
        close(server_socket);
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Errore: impossibile fare bind sulla porta %d\n", port);
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Errore: impossibile mettere in ascolto il socket\n");
        close(server_socket);
        return -1;
    }

    printf("Server in ascolto su porta %d\n", port);
    return (int)server_socket;
}

int accept_client_connection(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    SOCKET client_socket = accept((SOCKET)server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_socket == INVALID_SOCKET) {
        printf("Errore: impossibile accettare connessione\n");
        return -1;
    }

    if (set_nonblocking((int)client_socket) == SOCKET_ERROR) {
        printf("Errore: impossibile impostare non-blocking\n");
        close(client_socket);
        return -1;
    }

    printf("Client connesso da %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return (int)client_socket;
}

/* NB: questa funzione non e' thread-safe rispetto a scritture concorrenti sullo
   stesso socket_fd. Il chiamante (main.c) deve serializzare gli invii verso lo
   stesso client con il mutex Player.send_mutex quando piu' thread possono
   scrivere sullo stesso socket (es. notifiche asincrone all'avversario). */
int send_message(int socket_fd, const Message* msg) {
    char* json_str = message_to_json(msg);
    if (json_str == NULL) {
        printf("Errore: impossibile convertire messaggio in JSON\n");
        return 0;
    }

    char buffer[4096];
    snprintf(buffer, sizeof(buffer), "%s\n", json_str);

    int bytes_sent = send((SOCKET)socket_fd, buffer, (int)strlen(buffer), 0);

    free(json_str);

    if (bytes_sent == SOCKET_ERROR) {
        printf("Errore: impossibile inviare messaggio\n");
        return 0;
    }

    return 1;
}

Message receive_message(int socket_fd) {
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));

    int bytes_received = recv((SOCKET)socket_fd, buffer, sizeof(buffer) - 1, 0);

    Message msg;
    memset(&msg, 0, sizeof(Message));

    if (bytes_received < 0) {
        /* Socket non-bloccante: nessun dato disponibile (o errore reale, ignorato qui) */
        strcpy(msg.error, "");
        return msg;
    }

    if (bytes_received == 0) {
        strcpy(msg.error, "Connessione chiusa");
        return msg;
    }

    msg = parse_json_message(buffer);
    return msg;
}

void close_connection(int socket_fd) {
    close((SOCKET)socket_fd);
}
