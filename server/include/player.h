#ifndef PLAYER_H
#define PLAYER_H

#include <time.h>
#include <pthread.h>

typedef enum {
    PLAYER_CONNECTED,
    PLAYER_WAITING,
    PLAYER_PLAYING,
    PLAYER_DISCONNECTED
} PlayerState;

typedef struct {
    int player_id;
    int socket_fd;
    char username[64];
    PlayerState state;
    time_t last_activity;
    char current_match_id[36];   /* era "int": bug, i match_id sono stringhe */
    pthread_mutex_t send_mutex;  /* protegge send_message() da scritture concorrenti
                                     sullo stesso socket (thread del giocatore +
                                     thread che notificano eventi dell'avversario) */
} Player;

#endif
