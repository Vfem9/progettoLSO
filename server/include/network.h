#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"

int create_server_socket(int port);
int accept_client_connection(int server_socket);
int send_message(int socket_fd, const Message* msg);
Message receive_message(int socket_fd);
void close_connection(int socket_fd);

#endif
