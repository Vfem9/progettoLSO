#ifndef PLAYER_H
#define PLAYER_H

#include <time.h>

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
    int current_match_id;
} Player;

#endif