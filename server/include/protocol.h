#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MSG_TYPE_REQUEST        "REQUEST"
#define MSG_TYPE_RESPONSE       "RESPONSE"
#define MSG_TYPE_NOTIFICATION   "NOTIFICATION"

#define ACTION_REGISTER         "register"
#define ACTION_CREATE_MATCH     "create_match"
#define ACTION_LIST_MATCHES     "list_matches"
/* ACTION_JOIN_MATCH: prima faceva entrare subito nella partita (join
   istantaneo). FIX: ora e' solo una RICHIESTA di partecipazione - il
   creatore deve accettarla esplicitamente (ACTION_ACCEPT_JOIN/REJECT_JOIN)
   prima che la partita inizi davvero, come richiesto dall'utente. */
#define ACTION_JOIN_MATCH       "join_match"
#define ACTION_ACCEPT_JOIN      "accept_join"
#define ACTION_REJECT_JOIN      "reject_join"
/* Annulla una richiesta di partecipazione gia' inviata (in attesa di
   decisione o ancora in coda), mandata da chi l'ha fatta (non dal creatore). */
#define ACTION_CANCEL_JOIN      "cancel_join"
#define ACTION_MOVE             "move"
#define ACTION_QUIT_MATCH       "quit_match"
#define ACTION_REMATCH          "rematch"

/* Azioni usate solo in messaggi di tipo NOTIFICATION (server -> client, non richieste) */
#define ACTION_MATCH_STARTED    "match_started"
#define ACTION_OPPONENT_LEFT    "opponent_left"
#define ACTION_MATCH_CLOSED     "match_closed"   /* l'avversario ha lasciato una partita
                                                     gia' finita (ha rifiutato la rivincita
                                                     o e' tornato alla lobby) */
/* Al creatore: qualcuno ha chiesto di partecipare alla sua partita. */
#define ACTION_JOIN_REQUEST           "join_request"
/* A chi aveva richiesto di partecipare: il creatore ha rifiutato. */
#define ACTION_JOIN_REQUEST_REJECTED  "join_request_rejected"
/* A chi era in attesa/in coda: la richiesta non e' piu' valida (la partita
   e' iniziata con qualcun altro, oppure il creatore ha chiuso la partita). */
#define ACTION_JOIN_REQUEST_OBSOLETE  "join_request_obsolete"
/* A chi era in coda: la sua richiesta e' stata promossa a "in decisione"
   (quella davanti e' stata rifiutata/annullata). Serve solo a far sparire
   da solo, lato client, il popup "il creatore sta valutando un'altra
   richiesta" mostrato quando si era finiti in coda. */
#define ACTION_JOIN_REQUEST_PROMOTED  "join_request_promoted"
/* Broadcast a TUTTI i client connessi tranne i due partecipanti, mandato
   quando una partita passa da "in attesa"/"in decisione" a "in corso":
   la traccia richiede che gli altri client vengano informati che quella
   partita non e' piu' disponibile, non solo che smetta di comparire al
   prossimo refresh della lista. */
#define ACTION_MATCH_UNAVAILABLE      "match_unavailable"

typedef struct {
    char type[32];
    char action[32];
    char msg_id[64];
    char payload[1024];
    char error[256];
} Message;

Message parse_json_message(const char* json_string);
char* message_to_json(const Message* msg);

#endif
