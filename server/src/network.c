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
    free(json_str);

    /* FIX (robustezza, scoperto durante una revisione approfondita): il
       socket e' non bloccante (set_nonblocking in accept_client_connection),
       quindi send() puo' legittimamente scrivere MENO byte di quanti
       richiesti (invio parziale) o fallire momentaneamente perche' il
       buffer di invio del kernel e' pieno, senza che sia un errore reale.
       La vecchia versione faceva una singola send() e considerava
       "riuscito" qualsiasi risultato diverso da SOCKET_ERROR: un invio
       parziale avrebbe perso silenziosamente il resto del messaggio
       (compreso il '\n' finale), corrompendo il framing lato client, che
       si aspetta ogni riga terminata correttamente. Con i messaggi piccoli
       di questo protocollo (sotto i 2KB, ben sotto i buffer di invio tipici
       del sistema operativo) questo non si e' mai manifestato nei nostri
       test, ma non era comunque garantito. Ora ritentiamo finche' tutto il
       messaggio non e' stato inviato, con un limite di tentativi per non
       restare bloccati per sempre se la connessione e' davvero caduta. */
    size_t total_len = strlen(buffer);
    size_t sent_so_far = 0;
    int stall_count = 0;

    while (sent_so_far < total_len) {
        int bytes_sent = send((SOCKET)socket_fd, buffer + sent_so_far, (int)(total_len - sent_so_far), 0);

        if (bytes_sent > 0) {
            sent_so_far += (size_t)bytes_sent;
            stall_count = 0;
            continue;
        }

        stall_count++;
        if (stall_count > 2000) {
            printf("Errore: impossibile inviare messaggio (connessione bloccata o caduta)\n");
            return 0;
        }
    }

    return 1;
}

void recv_buffer_init(RecvBuffer* rb) {
    rb->len = 0;
    rb->buf[0] = '\0';
}

/* FIX (framing dei messaggi): vedi il commento su RecvBuffer in network.h.
   Ogni chiamata:
   1) se il buffer di riassemblaggio contiene gia' un '\n' (avanzato da una
      chiamata precedente, es. due messaggi arrivati insieme), estrae ed
      elabora SUBITO quella riga, senza toccare la rete;
   2) altrimenti prova a leggere altri byte dal socket e li accoda al
      buffer, poi ricontrolla se ora c'e' un '\n';
   3) se ancora non c'e' una riga completa, ritorna un Message vuoto (il
      chiamante in main.c lo interpreta come "nessun messaggio pronto",
      esattamente come prima in caso di nessun dato disponibile) SENZA
      scartare i byte gia' accumulati: verranno completati alla prossima
      chiamata. */
Message receive_message(int socket_fd, RecvBuffer* rb) {
    Message msg;
    memset(&msg, 0, sizeof(Message));

    char* newline = (char*)memchr(rb->buf, '\n', (size_t)rb->len);

    if (newline == NULL) {
        char chunk[2048];
        int bytes_received = recv((SOCKET)socket_fd, chunk, sizeof(chunk), 0);

        if (bytes_received < 0) {
            /* Socket non-bloccante: nessun dato disponibile adesso (o errore
               reale, ignorato qui come faceva gia' il codice originale). */
            return msg;
        }

        if (bytes_received == 0) {
            strcpy(msg.error, "Connessione chiusa");
            return msg;
        }

        /* Protezione overflow: un client che manda un "messaggio" senza mai
           un '\n' piu' grande del buffer non deve poter scrivere fuori dai
           limiti. Nel protocollo normale non dovrebbe mai succedere (i
           messaggi restano ben sotto 2KB), quindi trattarlo come un errore
           di protocollo e scartare il buffer e' una risposta ragionevole. */
        if (rb->len + bytes_received > (int)sizeof(rb->buf) - 1) {
            rb->len = 0;
            rb->buf[0] = '\0';
            strcpy(msg.error, "Messaggio malformato o troppo grande");
            return msg;
        }

        memcpy(rb->buf + rb->len, chunk, (size_t)bytes_received);
        rb->len += bytes_received;
        rb->buf[rb->len] = '\0';

        newline = (char*)memchr(rb->buf, '\n', (size_t)rb->len);
        if (newline == NULL) {
            /* Messaggio ancora incompleto: aspettiamo il resto alla
               prossima chiamata, senza scartare nulla. */
            return msg;
        }
    }

    int line_len = (int)(newline - rb->buf);
    char line[4096];
    int copy_len = (line_len < (int)sizeof(line) - 1) ? line_len : (int)sizeof(line) - 1;
    memcpy(line, rb->buf, (size_t)copy_len);
    line[copy_len] = '\0';

    /* Consuma la riga (incluso il '\n') e sposta all'inizio del buffer
       l'eventuale resto gia' arrivato (es. l'inizio del messaggio
       successivo, se erano arrivati insieme). */
    int consumed = line_len + 1;
    int remaining = rb->len - consumed;
    if (remaining > 0) {
        memmove(rb->buf, rb->buf + consumed, (size_t)remaining);
    }
    rb->len = remaining;
    rb->buf[rb->len] = '\0';

    msg = parse_json_message(line);
    return msg;
}

void close_connection(int socket_fd) {
    close((SOCKET)socket_fd);
}
