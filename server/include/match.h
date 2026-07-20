#ifndef MATCH_H
#define MATCH_H

#include <pthread.h>
#include <time.h>
#include "game.h"
#include "player.h"

typedef enum {
    WAITING_FOR_OPPONENT,
    JOIN_PENDING,
    ACTIVE,
    FINISHED,
    ABANDONED
} MatchState;

typedef struct {
    char match_id[36];
    int creator_id;
    int player1_id;
    int player2_id;
    Board board;
    int current_turn;
    MatchState state;
    time_t created_at;
    time_t finished_at;
    pthread_mutex_t mutex;
} Match;

Match* create_match(int creator_id, const char* match_id);
void destroy_match(Match* m);

#endif