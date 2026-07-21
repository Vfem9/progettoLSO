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

    m->pending_requester_id = -1;
    m->join_queue_count = 0;
    m->rejected_count = 0;
    m->winner_id = -1;
    m->rematch_vote_p1 = 0;
    m->rematch_vote_p2 = 0;
    m->withdrawn_requester_id = -1;

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

MoveResult apply_match_move(Match* m, int player_id, int column) {
    MoveResult mr;
    mr.row = -1;
    mr.symbol = 0;
    mr.next_turn = 0;

    pthread_mutex_lock(&m->mutex);

    if (m->state != ACTIVE) {
        printf("Errore: la partita %s non e' attiva\n", m->match_id);
        pthread_mutex_unlock(&m->mutex);
        mr.result = -3;
        return mr;
    }

    int expected_player = (m->current_turn == 1) ? m->player1_id : m->player2_id;
    if (player_id != expected_player) {
        printf("Errore: non e' il turno del giocatore %d\n", player_id);
        pthread_mutex_unlock(&m->mutex);
        mr.result = -1;
        return mr;
    }

    int symbol = m->current_turn;
    int row = apply_move(m->board, column, symbol);
    if (row == -1) {
        printf("Errore: mossa non valida in colonna %d\n", column);
        pthread_mutex_unlock(&m->mutex);
        mr.result = -2;
        return mr;
    }

    mr.row = row;
    mr.symbol = symbol;

    if (check_win(m->board, row, column, symbol)) {
        printf("Giocatore %d ha vinto!\n", player_id);
        m->state = FINISHED;
        m->finished_at = time(NULL);
        mr.result = 1;
        mr.next_turn = m->current_turn;
        pthread_mutex_unlock(&m->mutex);
        return mr;
    }

    if (check_draw(m->board)) {
        printf("Partita %s: pareggio!\n", m->match_id);
        m->state = FINISHED;
        m->finished_at = time(NULL);
        mr.result = 2;
        mr.next_turn = m->current_turn;
        pthread_mutex_unlock(&m->mutex);
        return mr;
    }

    m->current_turn = (m->current_turn == 1) ? 2 : 1;
    mr.result = 0;
    mr.next_turn = m->current_turn;

    printf("Mossa applicata in colonna %d (riga %d). Tocca al giocatore %d\n", column, row, m->current_turn);

    pthread_mutex_unlock(&m->mutex);
    return mr;
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

int get_opponent_id(Match* m, int player_id) {
    if (player_id == m->player1_id) {
        return m->player2_id;
    }
    if (player_id == m->player2_id) {
        return m->player1_id;
    }
    return -1;
}

int is_player_in_match(Match* m, int player_id) {
    return (player_id == m->player1_id) || (player_id == m->player2_id);
}

void reset_match_for_rematch(Match* m) {
    pthread_mutex_lock(&m->mutex);

    init_board(m->board);
    m->current_turn = 1;
    m->state = ACTIVE;
    m->finished_at = 0;
    m->pending_requester_id = -1;
    m->join_queue_count = 0;
    m->rejected_count = 0;
    m->winner_id = -1;
    m->rematch_vote_p1 = 0;
    m->rematch_vote_p2 = 0;
    m->withdrawn_requester_id = -1;

    printf("Partita %s: rivincita, board azzerata\n", m->match_id);

    pthread_mutex_unlock(&m->mutex);
}

void open_new_session_as_owner(Match* m, int new_owner_id) {
    pthread_mutex_lock(&m->mutex);

    init_board(m->board);
    m->current_turn = 1;
    m->state = WAITING_FOR_OPPONENT;
    m->finished_at = 0;
    /* FIX (fedelta' alla traccia): "il vincitore... diventa automaticamente
       il nuovo proprietario della partita se non lo era gia'". Trasferiamo
       qui la proprieta' (creator_id/player1_id) al vincitore e liberiamo lo
       slot player2: la partita torna una partita "nuova" a tutti gli
       effetti, aperta in lobby a chiunque, non necessariamente allo stesso
       avversario di prima. */
    m->creator_id = new_owner_id;
    m->player1_id = new_owner_id;
    m->player2_id = -1;
    m->winner_id = -1;
    m->pending_requester_id = -1;
    m->join_queue_count = 0;
    m->rejected_count = 0;
    m->rematch_vote_p1 = 0;
    m->rematch_vote_p2 = 0;
    m->withdrawn_requester_id = -1;

    printf("Partita %s: nuova sessione aperta dal giocatore %d (nuovo proprietario)\n", m->match_id, new_owner_id);

    pthread_mutex_unlock(&m->mutex);
}

/* ---------- Gestione richieste di partecipazione ---------- */

int is_player_rejected(Match* m, int player_id) {
    for (int i = 0; i < m->rejected_count; i++) {
        if (m->rejected_players[i] == player_id) return 1;
    }
    return 0;
}

void add_rejected_player(Match* m, int player_id) {
    if (is_player_rejected(m, player_id)) return;
    if (m->rejected_count < MAX_JOIN_QUEUE) {
        m->rejected_players[m->rejected_count++] = player_id;
    }
}

int enqueue_join_request(Match* m, int player_id) {
    if (m->join_queue_count >= MAX_JOIN_QUEUE) return 0;
    m->join_queue[m->join_queue_count++] = player_id;
    return 1;
}

int dequeue_join_request(Match* m) {
    if (m->join_queue_count <= 0) return -1;
    int next_id = m->join_queue[0];
    /* Shift a sinistra (coda piccola, semplice e sufficiente qui). */
    for (int i = 1; i < m->join_queue_count; i++) {
        m->join_queue[i - 1] = m->join_queue[i];
    }
    m->join_queue_count--;
    return next_id;
}

int remove_from_join_queue(Match* m, int player_id) {
    for (int i = 0; i < m->join_queue_count; i++) {
        if (m->join_queue[i] == player_id) {
            for (int j = i + 1; j < m->join_queue_count; j++) {
                m->join_queue[j - 1] = m->join_queue[j];
            }
            m->join_queue_count--;
            return 1;
        }
    }
    return 0;
}
