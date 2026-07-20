#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../include/network.h"

// Inizializza Winsock (necessario su Windows)
void init_winsock() {
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

// Crea il socket server
int create_server_socket(int port) {
    init_winsock();
    
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        printf("Errore: impossibile creare socket\n");
        return -1;
    }
    
   
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
  
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Errore: impossibile fare bind sulla porta %d\n", port);
        closesocket(server_socket);
        return -1;
    }
    
    // Mette il socket in ascolto
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        printf("Errore: impossibile mettere in ascolto il socket\n");
        closesocket(server_socket);
        return -1;
    }
    
    printf("Server in ascolto su porta %d\n", port);
    return (int)server_socket;
}

// Accetta una nuova connessione da un client
int accept_client_connection(int server_socket) {
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    
    SOCKET client_socket = accept((SOCKET)server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_socket == INVALID_SOCKET) {
        printf("Errore: impossibile accettare connessione\n");
        return -1;
    }
    
    printf("Client connesso da %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return (int)client_socket;
}

// Invia un messaggio a un client
int send_message(int socket_fd, const Message* msg) {
    // Converte il messaggio struct in JSON
    char* json_str = message_to_json(msg);
    if (json_str == NULL) {
        printf("Errore: impossibile convertire messaggio in JSON\n");
        return 0;
    }
    
    // Invia il JSON sul socket
    int bytes_sent = send((SOCKET)socket_fd, json_str, (int)strlen(json_str), 0);
    free(json_str);
    
    if (bytes_sent == SOCKET_ERROR) {
        printf("Errore: impossibile inviare messaggio\n");
        return 0;
    }
    
    return 1;
}

// Riceve un messaggio da un client
Message receive_message(int socket_fd) {
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    
    // Riceve i dati dal socket
    int bytes_received = recv((SOCKET)socket_fd, buffer, sizeof(buffer) - 1, 0);
    
    Message msg;
    memset(&msg, 0, sizeof(Message));
    
    if (bytes_received <= 0) {
        strcpy_s(msg.error, sizeof(msg.error), "Errore ricezione messaggio");
        return msg;
    }
    
    // Parsa il JSON e lo trasforma in struct Message
    msg = parse_json_message(buffer);
    return msg;
}

// Chiude la connessione con un client
void close_connection(int socket_fd) {
    closesocket((SOCKET)socket_fd);
}