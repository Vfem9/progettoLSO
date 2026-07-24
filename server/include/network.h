#ifndef NETWORK_H
#define NETWORK_H

#include "protocol.h"

/* FIX (framing dei messaggi): la connessione TCP e' uno stream di byte, non
   ha confini di messaggio. Prima receive_message() faceva una singola
   recv() e trattava TUTTO cio' che arrivava come UN messaggio completo. Se
   due messaggi del client arrivavano ravvicinati (es. doppio click) potevano
   finire nello stesso recv() concatenati ("{...}\n{...}\n"): il parser
   (basato su strstr, che trova solo la PRIMA occorrenza di ogni campo)
   leggeva solo il primo e il secondo andava perso in silenzio, senza che
   ne' client ne' server se ne accorgessero. RecvBuffer e' un buffer di
   riassemblaggio per-connessione (uno per thread/client, nessuna
   condivisione tra thread quindi nessun mutex necessario) che accumula i
   byte ricevuti e restituisce un messaggio alla volta, delimitato da '\n'
   (lo stesso delimitatore che il client usa gia' in scrittura e che
   BufferedReader.readLine() usa gia' correttamente in lettura sul client). */
typedef struct {
    char buf[4096];
    int len;
} RecvBuffer;

void recv_buffer_init(RecvBuffer* rb);

int create_server_socket(int port);
int accept_client_connection(int server_socket);
int send_message(int socket_fd, const Message* msg);
Message receive_message(int socket_fd, RecvBuffer* rb);
void close_connection(int socket_fd);

#endif
