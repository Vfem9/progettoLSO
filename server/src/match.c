#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/match.h"

// Crea una nuova partita e la inizializza
Match* create_match(int creator_id, const char* match_id) {
    Match* m = (Match*)malloc(sizeof(Match));
    if (m == NULL) return NULL;
    
    // Inizializza tutti i campi della partita
    strcpy_s(m->match_id, sizeof(m->match_id), match_id);
    m->creator_id = creator_id;
    m->player1_id = creator_id;
    m->player2_id = -1; // Nessun secondo giocatore ancora
    m->current_turn = 1; // Tocca al player1
    m->state = WAITING_FOR_OPPONENT;
    m->created_at = time(NULL);
    m->finished_at = 0;
    
    // Inizializza il board vuoto
    init_board(m->board);
    
    // Crea il mutex che protegge questa partita
    pthread_mutex_init(&m->mutex, NULL);
    
    printf("Partita creata: %s dal giocatore %d\n", match_id, creator_id);
    return m;
}

// Libera la memoria della partita
void destroy_match(Match* m) {
    if (m == NULL) return;
    
    // Distrugge il mutex
    pthread_mutex_destroy(&m->mutex);
    
    // Libera la memoria
    free(m);
}

// Aggiunge il secondo giocatore alla partita
int add_player_to_match(Match* m, int player_id) {
    // Acquisisce il lucchetto della partita
    pthread_mutex_lock(&m->mutex);
    
    // Se c'è già un secondo giocatore, non può aggiungerne un altro
    if (m->player2_id != -1) {
        pthread_mutex_unlock(&m->mutex);
        return 0;
    }
    
    // Aggiunge il giocatore
    m->player2_id = player_id;
    m->state = ACTIVE; // La partita può iniziare
    
    printf("Giocatore %d aggiunto alla partita %s. Inizio gioco!\n", player_id, m->match_id);
    
    pthread_mutex_unlock(&m->mutex);
    return 1;
}

// Applica una mossa alla partita
int apply_match_move(Match* m, int player_id, int column) {
    pthread_mutex_lock(&m->mutex);
    
    // Verifica che sia il turno del giocatore
    int expected_player = (m->current_turn == 1) ? m->player1_id : m->player2_id;
    if (player_id != expected_player) {
        printf("Errore: non è il turno del giocatore %d\n", player_id);
        pthread_mutex_unlock(&m->mutex);
        return -1;
    }
    
    // Valida e applica la mossa
    int row = apply_move(m->board, column, m->current_turn);
    if (row == -1) {
        printf("Errore: mossa non valida in colonna %d\n", column);
        pthread_mutex_unlock(&m->mutex);
        return -2;
    }
    
    // Controlla se il giocatore ha vinto
    if (check_win(m->board, row, column, m->current_turn)) {
        printf("Giocatore %d ha vinto!\n", player_id);
        m->state = FINISHED;
        m->finished_at = time(NULL);
        pthread_mutex_unlock(&m->mutex);
        return 1; // Vittoria
    }
    
    // Controlla se è pareggio
    if (check_draw(m->board)) {
        printf("Partita %s: pareggio!\n", m->match_id);
        m->state = FINISHED;
        m->finished_at = time(NULL);
        pthread_mutex_unlock(&m->mutex);
        return 2; // Pareggio
    }
    
    // Passa il turno all'altro giocatore
    m->current_turn = (m->current_turn == 1) ? 2 : 1;
    
    printf("Mossa applicata in colonna %d (riga %d). Tocca al giocatore %d\n", column, row, m->current_turn);
    
    pthread_mutex_unlock(&m->mutex);
    return 0; // Gioco continua
}

// Verifica se la partita è ancora attiva
int is_match_active(Match* m) {
    int active;
    pthread_mutex_lock(&m->mutex);
    active = (m->state == ACTIVE);
    pthread_mutex_unlock(&m->mutex);
    return active;
}

// Abbandona la partita (giocatore si disconnette)
void abandon_match(Match* m, int player_id) {
    pthread_mutex_lock(&m->mutex);
    
    m->state = ABANDONED;
    m->finished_at = time(NULL);
    
    // L'altro giocatore vince a tavolino
    int winner = (player_id == m->player1_id) ? m->player2_id : m->player1_id;
    printf("Giocatore %d ha abbandonato. Giocatore %d vince!\n", player_id, winner);
    
    pthread_mutex_unlock(&m->mutex);
}