#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/match.h"

Match* create_match(int creator_id, const char* match_id) {
    Match* m = (Match*)malloc(sizeof(Match));
    if (m == NULL) return NULL;
    
    strcpy(m->match_id, match_id);
    m->creator_id = creator_id;
    m->player1_id = creator_id;
    m->player2_id = -1;
    m->current_turn = 1;
    m->state = WAITING_FOR_OPPONENT;
    m->created_at = time(NULL);
    m->finished_at = 0;
    
    init_board(m->board);
    pthread_mutex_init(&m->mutex, NULL);
    
    printf("Partita creata: %s dal giocatore %d\n", match_id, creator_id);
    return m;
}

void destroy_match(Match* m) {
    if (m == NULL) return;
    pthread_mutex_destroy(&m->mutex);
    free(m);
}

int add_player_to_match(Match* m, int player_id) {
    pthread_mutex_lock(&m->mutex);
    
    if (m->player2_id != -1) {
        pthread_mutex_unlock(&m->mutex);
        return 0;
    }
    
    m->player2_id = player_id;
    m->state = ACTIVE;
    
    printf("Giocatore %d aggiunto alla partita %s. Inizio gioco!\n", player_id, m->match_id);
    
    pthread_mutex_unlock(&m->mutex);
    return 1;
}

int apply_match_move(Match* m, int player_id, int column) {
    pthread_mutex_lock(&m->mutex);
    
    int expected_player = (m->current_turn == 1) ? m->player1_id : m->player2_id;
    if (player_id != expected_player) {
        printf("Errore: non è il turno del giocatore %d\n", player_id);
        pthread_mutex_unlock(&m->mutex);
        return -1;
    }
    
    int row = apply_move(m->board, column, m->current_turn);
    if (row == -1) {
        printf("Errore: mossa non valida in colonna %d\n", column);
        pthread_mutex_unlock(&m->mutex);
        return -2;
    }
    
    if (check_win(m->board, row, column, m->current_turn)) {
        printf("Giocatore %d ha vinto!\n", player_id);
        m->state = FINISHED;
        m->finished_at = time(NULL);
        pthread_mutex_unlock(&m->mutex);
        return 1;
    }
    
    if (check_draw(m->board)) {
        printf("Partita %s: pareggio!\n", m->match_id);
        m->state = FINISHED;
        m->finished_at = time(NULL);
        pthread_mutex_unlock(&m->mutex);
        return 2;
    }
    
    m->current_turn = (m->current_turn == 1) ? 2 : 1;
    
    printf("Mossa applicata in colonna %d (riga %d). Tocca al giocatore %d\n", column, row, m->current_turn);
    
    pthread_mutex_unlock(&m->mutex);
    return 0;
}

int is_match_active(Match* m) {
    int active;
    pthread_mutex_lock(&m->mutex);
    active = (m->state == ACTIVE);
    pthread_mutex_unlock(&m->mutex);
    return active;
}

void abandon_match(Match* m, int player_id) {
    pthread_mutex_lock(&m->mutex);
    
    m->state = ABANDONED;
    m->finished_at = time(NULL);
    
    int winner = (player_id == m->player1_id) ? m->player2_id : m->player1_id;
    printf("Giocatore %d ha abbandonato. Giocatore %d vince!\n", player_id, winner);
    
    pthread_mutex_unlock(&m->mutex);
}