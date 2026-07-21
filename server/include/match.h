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

/* Numero massimo di richieste di partecipazione che possono restare in coda
   per una stessa partita mentre il creatore decide su un'altra. Ampiamente
   sufficiente: non ci si aspetta piu' di qualche client in attesa. */
#define MAX_JOIN_QUEUE 16

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

    /* FIX: prima chi cliccava "unisciti" entrava subito in partita. Ora e'
       solo una richiesta che il creatore deve accettare/rifiutare
       esplicitamente. Questi campi tracciano la richiesta attualmente in
       decisione, quelle in coda (se il creatore sta gia' valutando qualcun
       altro) e chi e' gia' stato rifiutato (non puo' ri-richiedere). */
    int pending_requester_id;                  /* -1 se nessuna richiesta in decisione */
    int join_queue[MAX_JOIN_QUEUE];
    int join_queue_count;
    int rejected_players[MAX_JOIN_QUEUE];
    int rejected_count;

    /* FIX: se chi ha fatto la richiesta la annulla (ACTION_CANCEL_JOIN)
       mentre il creatore sta ancora decidendo (dialog si/no gia' aperto sul
       suo client), quando il creatore risponde comunque non c'e' piu'
       nessuna richiesta da accettare/rifiutare. Prima il creatore vedeva un
       generico "Nessuna richiesta da accettare/rifiutare"; ora ricordiamo
       CHI ha ritirato, cosi' possiamo dirglielo esplicitamente. Viene
       azzerato (-1) non appena una NUOVA richiesta diventa quella attiva
       (la nuova notifica la supera comunque). */
    int withdrawn_requester_id;

    /* FIX: per rispettare fedelmente la traccia sul rigioco, il server deve
       ricordarsi CHI ha vinto (o se e' stato un pareggio) anche dopo che la
       partita e' FINISHED, per poter distinguere: (a) vittoria/sconfitta ->
       solo il vincitore puo' aprire una nuova sessione, il perdente e'
       obbligato ad uscire senza scelta; (b) pareggio -> serve il consenso
       di ENTRAMBI i giocatori (voto congiunto) prima di far ripartire la
       partita. */
    int winner_id;           /* -1 se pareggio (o partita non ancora conclusa) */
    int rematch_vote_p1;      /* 0 = non ha ancora votato, 1 = ha votato si (solo pareggio) */
    int rematch_vote_p2;
} Match;

/* Risultato di una mossa: incapsula tutto cio' che serve a main.c per costruire
   la notifica da mandare a entrambi i giocatori, letto in modo atomico mentre
   il mutex del match e' ancora tenuto (evita race su m->current_turn letto
   dall'esterno dopo l'unlock). */
typedef struct {
    int result;     /* -3 match non attivo, -1 non e' il tuo turno,
                        -2 colonna non valida/piena, 0 ok, 1 vittoria, 2 pareggio */
    int row;         /* riga in cui il disco e' caduto (valido se result >= 0) */
    int symbol;       /* 1 o 2: simbolo appena piazzato (valido se result >= 0) */
    int next_turn;     /* m->current_turn dopo la mossa (valido se result >= 0) */
} MoveResult;

Match* create_match(int creator_id, const char* match_id);
void destroy_match(Match* m);
int add_player_to_match(Match* m, int player_id);
MoveResult apply_match_move(Match* m, int player_id, int column);
int is_match_active(Match* m);
void abandon_match(Match* m, int player_id);

int get_opponent_id(Match* m, int player_id);
int is_player_in_match(Match* m, int player_id);
void reset_match_for_rematch(Match* m);

/* Gestione richieste di partecipazione (accetta/rifiuta). Il chiamante deve
   gia' tenere sync_lock_matches() (stesso modello degli altri accessi a Match). */
int is_player_rejected(Match* m, int player_id);
void add_rejected_player(Match* m, int player_id);
int enqueue_join_request(Match* m, int player_id);   /* 1 = messo in coda, 0 = coda piena */
int dequeue_join_request(Match* m);                  /* id del prossimo in coda, -1 se vuota */
int remove_from_join_queue(Match* m, int player_id); /* 1 = trovato e rimosso */

/* Il vincitore di una vittoria/sconfitta apre una NUOVA sessione sulla
   stessa partita, diventandone proprietario (creator_id/player1_id) se non
   lo era gia': la partita torna in stato di attesa, aperta a chiunque in
   lobby (non necessariamente allo stesso avversario di prima). */
void open_new_session_as_owner(Match* m, int new_owner_id);

#endif
