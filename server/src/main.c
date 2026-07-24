#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#ifdef _WIN32
    /* FIX: winsock2.h definisce gia' il tipo SOCKET (come intero a 64 bit,
       UINT_PTR). Il codice originale lo ridefiniva qui come "int", il che
       confliggeva con la vera definizione e mandava in errore la
       compilazione su Windows ("conflicting types for 'SOCKET'"). Il tipo
       SOCKET in realta' non serve in questo file: main.c maneggia sempre
       socket come "int" semplice (stessa convenzione di network.h), quindi
       basta includere winsock2.h senza ridefinire nulla. */
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/select.h>
#endif
#include "../include/game.h"
#include "../include/match.h"
#include "../include/player.h"
#include "../include/network.h"
#include "../include/protocol.h"
#include "../include/sync.h"

#define MAX_MATCHES 100
#define MAX_PLAYERS 100
#define SERVER_PORT 5000

Match* matches[MAX_MATCHES];
int matches_count = 0;

Player* players[MAX_PLAYERS];
int players_count = 0;

typedef struct {
    int client_socket;
    int player_id;
} ClientThreadArgs;

/* ---------- Funzioni di supporto ---------- */

/* Serializza la board in JSON tipo [[0,0,..],[..]]. Assume outsize congruo
   (usare almeno 256 byte: 6x7 celle "0," + parentesi e' ben sotto quella soglia). */
static void build_board_json(Board board, char* out, size_t outsize) {
    size_t pos = 0;
    if (outsize == 0) return;
    out[0] = '\0';

#define APPEND(str_literal) do { \
        size_t len = strlen(str_literal); \
        if (pos + len < outsize) { \
            memcpy(out + pos, str_literal, len); \
            pos += len; \
            out[pos] = '\0'; \
        } \
    } while (0)

    APPEND("[");
    for (int row = 0; row < BOARD_ROWS; row++) {
        APPEND("[");
        for (int col = 0; col < BOARD_COLS; col++) {
            char cell[8];
            snprintf(cell, sizeof(cell), "%d%s", board[row][col], (col < BOARD_COLS - 1) ? "," : "");
            APPEND(cell);
        }
        APPEND((row < BOARD_ROWS - 1) ? "]," : "]");
    }
    APPEND("]");
#undef APPEND
}

/* Cerca una partita per id. Il chiamante deve gia' tenere sync_lock_matches(). */
static Match* find_match_by_id(const char* match_id) {
    for (int i = 0; i < matches_count; i++) {
        if (strcmp(matches[i]->match_id, match_id) == 0) {
            return matches[i];
        }
    }
    return NULL;
}

/* Invia un Message gia' pronto al giocatore player_id, serializzando gli
   accessi al suo socket con il suo send_mutex (piu' thread possono provare
   a scrivergli: il suo stesso thread e i thread che notificano mosse
   dell'avversario). Puo' essere chiamata mentre si tiene sync_lock_matches()
   (ordine di lock rispettato: matches_mutex -> players_mutex -> send_mutex). */
static void send_response(int player_id, Message* msg) {
    sync_lock_players();
    Player* p = (player_id >= 0 && player_id < MAX_PLAYERS) ? players[player_id] : NULL;
    sync_unlock_players();

    if (p == NULL || p->state == PLAYER_DISCONNECTED) {
        return;
    }

    pthread_mutex_lock(&p->send_mutex);
    send_message(p->socket_fd, msg);
    pthread_mutex_unlock(&p->send_mutex);
}

/* Costruisce e invia una NOTIFICATION (messaggio non richiesto dal client,
   usato per avvisare l'avversario di mosse, inizio partita, disconnessioni...). */
static void send_notification(int player_id, const char* action, const char* payload) {
    Message notif;
    memset(&notif, 0, sizeof(Message));
    strcpy(notif.type, MSG_TYPE_NOTIFICATION);
    strncpy(notif.action, action, sizeof(notif.action) - 1);
    snprintf(notif.msg_id, sizeof(notif.msg_id), "srv_%ld_%d", (long)time(NULL), rand() % 100000);
    if (payload) {
        strncpy(notif.payload, payload, sizeof(notif.payload) - 1);
    }
    send_response(player_id, &notif);
}

/* Copia lo username del giocatore pid in out (stringa vuota/"Sconosciuto" se
   non trovato). Puo' essere chiamata mentre si tiene sync_lock_matches()
   (ordine di lock rispettato: matches_mutex -> players_mutex). */
static void get_username(int pid, char* out, size_t outsize) {
    if (outsize == 0) return;
    sync_lock_players();
    if (pid >= 0 && pid < MAX_PLAYERS && players[pid]) {
        strncpy(out, players[pid]->username, outsize - 1);
        out[outsize - 1] = '\0';
    } else {
        strncpy(out, "Sconosciuto", outsize - 1);
        out[outsize - 1] = '\0';
    }
    sync_unlock_players();
}

/* FIX: gli username ora sono scelti liberamente dall'utente (prima erano
   sempre generati in automatico dal client, quindi "al sicuro"). Se uno
   contenesse virgolette o backslash e venisse incollato cosi' com'e' dentro
   una stringa JSON, romperebbe il parsing lato client. Questo escaping
   minimo copre i casi piu' comuni (virgolette, backslash, caratteri di
   controllo). */
static void json_escape(const char* in, char* out, size_t outsize) {
    size_t j = 0;
    if (outsize == 0) return;
    for (size_t i = 0; in[i] != '\0' && j + 2 < outsize; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c < 0x20) {
            continue; /* salta i caratteri di controllo, per semplicita' */
        } else {
            out[j++] = (char)c;
        }
    }
    out[j] = '\0';
}

/* FIX: la traccia richiede che ogni giocatore possa creare una o piu'
   partite, ma "deve poter giocare attivamente solo a una partita alla
   volta". Prima non c'era alcun controllo lato server: un client poteva
   creare o richiedere di unirsi a piu' partite in parallelo. Questa
   funzione dice se player_id e' gia' "impegnato" altrove: come
   creatore/giocatore di una partita non ancora chiusa (in attesa, in
   decisione, in corso o appena conclusa ma non ancora lasciata), oppure
   come richiedente in decisione/in coda su un'altra partita. Il chiamante
   deve gia' tenere sync_lock_matches(). */
static int is_player_busy(int player_id) {
    for (int i = 0; i < matches_count; i++) {
        Match* m = matches[i];
        if (m->state == ABANDONED) continue;

        if (m->pending_requester_id == player_id) return 1;
        for (int qi = 0; qi < m->join_queue_count; qi++) {
            if (m->join_queue[qi] == player_id) return 1;
        }

        int is_participant = (m->player1_id == player_id || m->player2_id == player_id);

        if (m->state == WAITING_FOR_OPPONENT || m->state == JOIN_PENDING || m->state == ACTIVE) {
            if (is_participant) return 1;
        } else if (m->state == FINISHED && is_participant) {
            /* FIX: una partita FINISHED non deve tenere "occupato" per
               sempre il PERDENTE - e' gia' stato obbligato a uscire ed e'
               libero immediatamente, indipendentemente da quando (o se) il
               vincitore decidera' di aprire una nuova sessione. Solo chi ha
               ancora una decisione in sospeso resta "occupato": il
               vincitore (vittoria/sconfitta) oppure entrambi i giocatori
               (pareggio, finche' non votano o escono). */
            if (m->winner_id == -1 || m->winner_id == player_id) {
                return 1;
            }
        }
    }
    return 0;
}

/* FIX: la traccia richiede che, quando una partita passa "in corso" (o si
   conclude), gli altri client collegati vengano informati che non e' piu'
   disponibile. Prima questo succedeva solo indirettamente, al successivo
   refresh periodico (ogni 3s) della lista partite lato client. Questa
   funzione notifica subito TUTTI i giocatori connessi tranne i due
   partecipanti (exclude1/exclude2). Il chiamante deve gia' tenere
   sync_lock_matches(); la funzione acquisisce/rilascia sync_lock_players()
   internamente e NON la tiene mentre chiama send_notification() (che la
   acquisisce a sua volta), per rispettare l'ordine di lock documentato
   sopra ed evitare deadlock. */
static void broadcast_match_unavailable(const char* match_id, int exclude1, int exclude2) {
    char payload[100];
    snprintf(payload, sizeof(payload), "{\"match_id\":\"%s\"}", match_id);

    int ids[MAX_PLAYERS];
    int count = 0;

    sync_lock_players();
    for (int i = 0; i < players_count; i++) {
        Player* p = players[i];
        if (p == NULL || p->state == PLAYER_DISCONNECTED) continue;
        int pid = p->player_id;
        if (pid == exclude1 || pid == exclude2) continue;
        ids[count++] = pid;
    }
    sync_unlock_players();

    for (int i = 0; i < count; i++) {
        send_notification(ids[i], ACTION_MATCH_UNAVAILABLE, payload);
    }
}

/* ---------- Thread per ogni client ---------- */

void* client_thread_handler(void* args) {
    ClientThreadArgs* thread_args = (ClientThreadArgs*)args;
    int client_socket = thread_args->client_socket;
    int player_id = thread_args->player_id;
    free(thread_args);

    printf("Thread client %d avviato\n", player_id);

    /* FIX (framing dei messaggi): buffer di riassemblaggio dedicato a
       questa connessione (vive sullo stack del thread, un thread per
       client: nessuna condivisione, nessun mutex necessario). Vedi i
       commenti su RecvBuffer in network.h e dentro receive_message(). */
    RecvBuffer recv_buf;
    recv_buffer_init(&recv_buf);

    while (1) {
        /* FIX: se il buffer contiene gia' una riga completa (es. due
           messaggi del client arrivati insieme in una singola recv())
           elaboriamola subito, senza aspettare altro traffico di rete:
           select() qui sotto puo' dire "nessun dato pronto" anche quando
           in realta' abbiamo gia' un messaggio intero bufferizzato
           localmente, aspettato per errore fino al prossimo dato sul
           socket (che potrebbe non arrivare mai). */
        int has_buffered_line = (memchr(recv_buf.buf, '\n', (size_t)recv_buf.len) != NULL);

        if (!has_buffered_line) {
            fd_set readfds;
            struct timeval tv;

            FD_ZERO(&readfds);
            FD_SET(client_socket, &readfds);

            tv.tv_sec = 0;
            tv.tv_usec = 500000;

            int select_result = select(client_socket + 1, &readfds, NULL, NULL, &tv);

            if (select_result <= 0) {
                continue;
            }
        }

        Message msg = receive_message(client_socket, &recv_buf);

        /* FIX: l'ordine dei due controlli era invertito. Quando il client si
           disconnette, receive_message() ritorna un Message con type vuoto E
           error="Connessione chiusa": il controllo su type[0]=='\0' intercettava
           per primo e faceva "continue", saltando per sempre il break sotto.
           Risultato: la disconnessione non veniva mai rilevata, l'avversario
           restava in attesa a tempo indeterminato. Ora l'errore viene
           controllato per primo. */
        if (msg.error[0] != '\0') {
            printf("Client %d disconnesso\n", player_id);
            break;
        }

        if (msg.type[0] == '\0') {
            continue;
        }

        printf("Ricevuto: type=%s, action=%s da giocatore %d\n", msg.type, msg.action, player_id);

        Message response;
        memset(&response, 0, sizeof(Message));
        strcpy(response.type, MSG_TYPE_RESPONSE);
        strcpy(response.msg_id, msg.msg_id);

        if (strcmp(msg.action, ACTION_REGISTER) == 0) {
            char username[64] = {0};
            sscanf(msg.payload, "{\"username\":\"%63[^\"]\"}", username);

            sync_lock_players();
            if (player_id < MAX_PLAYERS && players[player_id]) {
                if (username[0] != '\0') {
                    strncpy(players[player_id]->username, username, sizeof(players[player_id]->username) - 1);
                }
                players[player_id]->state = PLAYER_WAITING;
            }
            sync_unlock_players();

            strcpy(response.action, ACTION_REGISTER);
            /* FIX: prima rispondeva sempre "player_id":1. Ora restituisce l'id reale
               assegnato alla connessione, indispensabile al client per sapere
               "chi e'" (e quindi il proprio colore/turno). */
            snprintf(response.payload, sizeof(response.payload), "{\"player_id\":%d,\"status\":\"ok\"}", player_id);

            printf("Giocatore registrato: %s (ID: %d)\n", username[0] ? username : "Unknown", player_id);
            send_response(player_id, &response);
        }
        else if (strcmp(msg.action, ACTION_CREATE_MATCH) == 0) {
            sync_lock_matches();

            if (matches_count >= MAX_MATCHES) {
                strcpy(response.error, "Too many matches");
                strcpy(response.action, ACTION_CREATE_MATCH);
                send_response(player_id, &response);
            } else if (is_player_busy(player_id)) {
                strcpy(response.error, "Sei gia' impegnato in un'altra partita");
                strcpy(response.action, ACTION_CREATE_MATCH);
                send_response(player_id, &response);
            } else {
                /* FIX: prima l'id usava solo player_id + timestamp al secondo,
                   quindi lo stesso giocatore che creava due partite nello
                   stesso secondo otteneva due id IDENTICI (scoperto durante
                   un test automatico: la seconda "creazione" risultava in
                   realta' un riferimento alla partita gia' esistente). Si
                   aggiunge matches_count, che aumenta ad ogni partita creata
                   sul server, come disambiguatore sempre univoco. */
                char match_id[36];
                snprintf(match_id, sizeof(match_id), "m_%d_%ld_%d", player_id, (long)time(NULL), matches_count);

                Match* new_match = create_match(player_id, match_id);
                matches[matches_count++] = new_match;

                sync_lock_players();
                if (player_id < MAX_PLAYERS && players[player_id]) {
                    strncpy(players[player_id]->current_match_id, match_id, sizeof(players[player_id]->current_match_id) - 1);
                }
                sync_unlock_players();

                char board_json[256];
                build_board_json(new_match->board, board_json, sizeof(board_json));

                snprintf(response.payload, sizeof(response.payload),
                    "{\"match_id\":\"%s\",\"status\":\"ok\",\"player_number\":1,\"board\":%s,\"current_turn\":%d}",
                    match_id, board_json, new_match->current_turn);
                strcpy(response.action, ACTION_CREATE_MATCH);

                printf("Partita creata: %s\n", match_id);
                send_response(player_id, &response);
            }

            sync_unlock_matches();
        }
        else if (strcmp(msg.action, ACTION_LIST_MATCHES) == 0) {
            sync_lock_matches();

            /* FIX (robustezza, scoperto durante una revisione approfondita):
               la vecchia versione accumulava le voci con strcat() dentro
               payload[2048] SENZA MAI controllare quanto spazio restava, poi
               copiava il risultato con strcpy() dentro response.payload che
               nel Message (protocol.h) e' grande solo 1024 byte. Con
               abbastanza partite visibili insieme (bastano circa 7-11, a
               seconda della lunghezza degli username) questo scriveva oltre
               i limiti di entrambi i buffer: comportamento indefinito, nel
               peggiore dei casi un crash del thread server. Costruiamo ora
               il payload direttamente dentro un buffer grande ESATTAMENTE
               come response.payload, con un append "sicuro" (stesso pattern
               gia' usato in build_board_json) che non scrive mai oltre i
               suoi limiti: se lo spazio finisce, smettiamo semplicemente di
               aggiungere altre partite alla lista invece di corrompere la
               memoria. Con il numero di partite di una sessione normale
               questo limite non viene mai nemmeno avvicinato. */
            char payload[sizeof(response.payload)];
            size_t pos = 0;
            payload[0] = '\0';

#define LIST_APPEND(str_val) do { \
            size_t len_ = strlen(str_val); \
            if (pos + len_ < sizeof(payload)) { \
                memcpy(payload + pos, str_val, len_); \
                pos += len_; \
                payload[pos] = '\0'; \
            } \
        } while (0)

            LIST_APPEND("{\"matches\":[");
            int first = 1;

            for (int i = 0; i < matches_count; i++) {
                /* FIX (fedelta' alla traccia): "gli altri client... vengono
                   solo messi a conoscenza che quella partita e' in corso e
                   non e' piu' possibile parteciparvi" - prima una partita
                   ACTIVE spariva del tutto dalla lista una volta iniziata
                   (broadcast_match_unavailable la toglieva soltanto). Ora
                   resta visibile anche mentre e' in corso, ma con uno stato
                   "active" che il client mostra come "IN CORSO" e non
                   permette di unirsi (il server rifiuta comunque qualsiasi
                   richiesta di join su una partita non WAITING/JOIN_PENDING,
                   vedi ACTION_JOIN_MATCH). Le partite FINISHED/ABANDONED
                   restano invece nascoste: non sono ne' in corso ne'
                   disponibili, mostrarle non aggiungerebbe informazione
                   utile e la lista crescerebbe all'infinito. */
                MatchState st = matches[i]->state;
                if (st == WAITING_FOR_OPPONENT || st == JOIN_PENDING || st == ACTIVE) {
                    /* FIX: la lista partite mostrava solo l'id numerico del
                       creatore. Aggiungiamo anche il suo username, cosi' la
                       lobby puo' mostrare "Partita di <nome>" invece di un
                       numero senza senso per l'utente. */
                    char creator_username[64];
                    get_username(matches[i]->creator_id, creator_username, sizeof(creator_username));
                    char creator_username_esc[128];
                    json_escape(creator_username, creator_username_esc, sizeof(creator_username_esc));

                    const char* status_str = (st == JOIN_PENDING) ? "pending" : (st == ACTIVE) ? "active" : "waiting";

                    char match_info[512];
                    snprintf(match_info, sizeof(match_info),
                        "%s{\"match_id\":\"%s\",\"creator\":%d,\"creator_username\":\"%s\",\"status\":\"%s\"}",
                        first ? "" : ",",
                        matches[i]->match_id, matches[i]->creator_id, creator_username_esc, status_str);

                    /* Se questa voce non ci sta piu' (lasciando spazio per il
                       "]}" finale), ci fermiamo qui: il client vedra' solo le
                       prime partite invece che tutte, decisamente meglio di
                       un buffer overflow lato server. */
                    if (pos + strlen(match_info) >= sizeof(payload) - 3) {
                        break;
                    }
                    LIST_APPEND(match_info);
                    first = 0;
                }
            }

            LIST_APPEND("]}");
#undef LIST_APPEND

            strcpy(response.action, ACTION_LIST_MATCHES);
            strcpy(response.payload, payload); /* sicuro: stessa dimensione di payload[] */

            printf("Lista partite inviata a giocatore %d\n", player_id);
            send_response(player_id, &response);

            sync_unlock_matches();
        }
        else if (strcmp(msg.action, ACTION_JOIN_MATCH) == 0) {
            /* FIX: prima questa azione faceva entrare subito in partita.
               Ora e' solo una RICHIESTA: se il creatore non sta gia'
               valutando qualcun altro, diventa la richiesta attiva e il
               creatore riceve una NOTIFICATION (ACTION_JOIN_REQUEST) con un
               popup si/no; altrimenti finisce in coda e verra' proposta al
               creatore in ordine, non appena si libera. */
            char match_id[36] = {0};
            sscanf(msg.payload, "{\"match_id\":\"%35[^\"]\"}", match_id);

            sync_lock_matches();

            Match* m = find_match_by_id(match_id);
            strcpy(response.action, ACTION_JOIN_MATCH);

            if (m == NULL) {
                strcpy(response.error, "Match not found");
                send_response(player_id, &response);
            } else if (m->state != WAITING_FOR_OPPONENT && m->state != JOIN_PENDING) {
                strcpy(response.error, "Match not available");
                send_response(player_id, &response);
            } else if (player_id == m->creator_id) {
                strcpy(response.error, "Non puoi unirti alla tua stessa partita");
                send_response(player_id, &response);
            } else if (is_player_rejected(m, player_id)) {
                strcpy(response.error, "Sei stato rifiutato da questa partita");
                send_response(player_id, &response);
            } else if (is_player_busy(player_id)) {
                strcpy(response.error, "Sei gia' impegnato in un'altra partita");
                send_response(player_id, &response);
            } else {
                char creator_username[64], creator_username_esc[128];
                get_username(m->creator_id, creator_username, sizeof(creator_username));
                json_escape(creator_username, creator_username_esc, sizeof(creator_username_esc));

                if (m->pending_requester_id == -1) {
                    /* Nessuno in decisione: la richiesta diventa attiva subito. */
                    m->pending_requester_id = player_id;
                    m->withdrawn_requester_id = -1;
                    m->state = JOIN_PENDING;

                    snprintf(response.payload, sizeof(response.payload),
                        "{\"status\":\"pending\",\"match_id\":\"%s\",\"creator_username\":\"%s\"}",
                        match_id, creator_username_esc);
                    send_response(player_id, &response);

                    char requester_username[64], requester_username_esc[128];
                    get_username(player_id, requester_username, sizeof(requester_username));
                    json_escape(requester_username, requester_username_esc, sizeof(requester_username_esc));

                    char notif_payload[300];
                    snprintf(notif_payload, sizeof(notif_payload),
                        "{\"match_id\":\"%s\",\"requester_id\":%d,\"requester_username\":\"%s\"}",
                        match_id, player_id, requester_username_esc);
                    send_notification(m->creator_id, ACTION_JOIN_REQUEST, notif_payload);

                    printf("Giocatore %d ha richiesto di unirsi a %s (in decisione)\n", player_id, match_id);
                } else if (enqueue_join_request(m, player_id)) {
                    /* Il creatore sta gia' valutando un'altra richiesta:
                       questa finisce in coda, verra' proposta se quella
                       viene rifiutata. */
                    snprintf(response.payload, sizeof(response.payload),
                        "{\"status\":\"queued\",\"match_id\":\"%s\",\"creator_username\":\"%s\"}",
                        match_id, creator_username_esc);
                    send_response(player_id, &response);

                    printf("Giocatore %d messo in coda per %s\n", player_id, match_id);
                } else {
                    strcpy(response.error, "Troppe richieste in attesa per questa partita, riprova piu' tardi");
                    send_response(player_id, &response);
                }
            }

            sync_unlock_matches();
        }
        else if (strcmp(msg.action, ACTION_ACCEPT_JOIN) == 0) {
            /* Il creatore accetta la richiesta di partecipazione attualmente
               in decisione per questa partita. */
            char match_id[36] = {0};
            sscanf(msg.payload, "{\"match_id\":\"%35[^\"]\"}", match_id);

            sync_lock_matches();

            Match* m = find_match_by_id(match_id);
            strcpy(response.action, ACTION_ACCEPT_JOIN);

            if (m == NULL || m->creator_id != player_id || m->pending_requester_id == -1) {
                /* FIX: se chi aveva fatto la richiesta l'ha nel frattempo
                   annullata (ACTION_CANCEL_JOIN) mentre il creatore stava
                   ancora decidendo, diciamoglielo esplicitamente invece del
                   generico "nessuna richiesta da accettare". */
                if (m != NULL && m->creator_id == player_id && m->withdrawn_requester_id != -1) {
                    char withdrawn_username[64], withdrawn_username_esc[128];
                    get_username(m->withdrawn_requester_id, withdrawn_username, sizeof(withdrawn_username));
                    json_escape(withdrawn_username, withdrawn_username_esc, sizeof(withdrawn_username_esc));
                    snprintf(response.error, sizeof(response.error), "%s ha ritirato la richiesta", withdrawn_username_esc);
                    m->withdrawn_requester_id = -1; /* consumato */
                } else {
                    strcpy(response.error, "Nessuna richiesta da accettare");
                }
                send_response(player_id, &response);
            } else {
                int accepted_id = m->pending_requester_id;
                m->pending_requester_id = -1;

                add_player_to_match(m, accepted_id); /* imposta player2_id e state = ACTIVE */

                sync_lock_players();
                if (accepted_id < MAX_PLAYERS && players[accepted_id]) {
                    strncpy(players[accepted_id]->current_match_id, match_id, sizeof(players[accepted_id]->current_match_id) - 1);
                }
                sync_unlock_players();

                strcpy(response.payload, "{\"status\":\"ok\"}");
                send_response(player_id, &response);

                char board_json[256];
                build_board_json(m->board, board_json, sizeof(board_json));

                char creator_username[64], accepted_username[64];
                get_username(m->player1_id, creator_username, sizeof(creator_username));
                get_username(accepted_id, accepted_username, sizeof(accepted_username));
                char creator_username_esc[128], accepted_username_esc[128];
                json_escape(creator_username, creator_username_esc, sizeof(creator_username_esc));
                json_escape(accepted_username, accepted_username_esc, sizeof(accepted_username_esc));

                /* FIX: prima ne' il creatore ne' chi si univa ricevevano lo
                   username dell'altro. Ora entrambi ricevono lo stato
                   completo della partita (board inclusa) via NOTIFICATION,
                   dato che l'ingresso in partita avviene solo ora, in modo
                   asincrono rispetto alla richiesta originale. */
                char notif_creator[640], notif_joiner[640];
                snprintf(notif_creator, sizeof(notif_creator),
                    "{\"match_id\":\"%s\",\"player_number\":1,\"opponent_id\":%d,\"opponent_username\":\"%s\",\"board\":%s,\"current_turn\":%d}",
                    match_id, accepted_id, accepted_username_esc, board_json, m->current_turn);
                snprintf(notif_joiner, sizeof(notif_joiner),
                    "{\"match_id\":\"%s\",\"player_number\":2,\"opponent_id\":%d,\"opponent_username\":\"%s\",\"board\":%s,\"current_turn\":%d}",
                    match_id, m->player1_id, creator_username_esc, board_json, m->current_turn);

                send_notification(m->player1_id, ACTION_MATCH_STARTED, notif_creator);
                send_notification(accepted_id, ACTION_MATCH_STARTED, notif_joiner);

                /* Chiunque fosse rimasto in coda: la partita e' iniziata con
                   qualcun altro, non e' piu' disponibile. */
                for (int qi = 0; qi < m->join_queue_count; qi++) {
                    char obsolete_payload[200];
                    snprintf(obsolete_payload, sizeof(obsolete_payload),
                        "{\"match_id\":\"%s\",\"reason\":\"started\"}", match_id);
                    send_notification(m->join_queue[qi], ACTION_JOIN_REQUEST_OBSOLETE, obsolete_payload);
                }
                m->join_queue_count = 0;

                /* FIX (fedelta' alla traccia): "gli altri client... vengono
                   solo messi a conoscenza che quella partita e' in corso e
                   non e' piu' possibile parteciparvi". Prima lo scoprivano
                   solo al refresh periodico della lista (fino a 3s dopo);
                   ora vengono avvisati subito. */
                broadcast_match_unavailable(match_id, m->player1_id, accepted_id);

                printf("Giocatore %d accettato nella partita %s\n", accepted_id, match_id);
            }

            sync_unlock_matches();
        }
        else if (strcmp(msg.action, ACTION_REJECT_JOIN) == 0) {
            /* Il creatore rifiuta la richiesta attualmente in decisione. Chi
               viene rifiutato non potra' ri-richiedere su questa stessa
               partita. Se c'e' qualcuno in coda, la sua richiesta viene
               proposta subito dopo al creatore. */
            char match_id[36] = {0};
            sscanf(msg.payload, "{\"match_id\":\"%35[^\"]\"}", match_id);

            sync_lock_matches();

            Match* m = find_match_by_id(match_id);
            strcpy(response.action, ACTION_REJECT_JOIN);

            if (m == NULL || m->creator_id != player_id || m->pending_requester_id == -1) {
                /* Stesso caso dell'accept: la richiesta potrebbe essere
                   stata ritirata nel frattempo. */
                if (m != NULL && m->creator_id == player_id && m->withdrawn_requester_id != -1) {
                    char withdrawn_username[64], withdrawn_username_esc[128];
                    get_username(m->withdrawn_requester_id, withdrawn_username, sizeof(withdrawn_username));
                    json_escape(withdrawn_username, withdrawn_username_esc, sizeof(withdrawn_username_esc));
                    snprintf(response.error, sizeof(response.error), "%s ha ritirato la richiesta", withdrawn_username_esc);
                    m->withdrawn_requester_id = -1; /* consumato */
                } else {
                    strcpy(response.error, "Nessuna richiesta da rifiutare");
                }
                send_response(player_id, &response);
            } else {
                int rejected_id = m->pending_requester_id;
                m->pending_requester_id = -1;
                add_rejected_player(m, rejected_id);

                strcpy(response.payload, "{\"status\":\"ok\"}");
                send_response(player_id, &response);

                char creator_username[64], creator_username_esc[128];
                get_username(m->creator_id, creator_username, sizeof(creator_username));
                json_escape(creator_username, creator_username_esc, sizeof(creator_username_esc));

                char reject_payload[300];
                snprintf(reject_payload, sizeof(reject_payload),
                    "{\"match_id\":\"%s\",\"creator_username\":\"%s\"}",
                    match_id, creator_username_esc);
                send_notification(rejected_id, ACTION_JOIN_REQUEST_REJECTED, reject_payload);

                int next_id = dequeue_join_request(m);
                if (next_id != -1) {
                    m->pending_requester_id = next_id;
                    m->withdrawn_requester_id = -1;
                    m->state = JOIN_PENDING;

                    char requester_username[64], requester_username_esc[128];
                    get_username(next_id, requester_username, sizeof(requester_username));
                    json_escape(requester_username, requester_username_esc, sizeof(requester_username_esc));

                    char notif_payload[300];
                    snprintf(notif_payload, sizeof(notif_payload),
                        "{\"match_id\":\"%s\",\"requester_id\":%d,\"requester_username\":\"%s\"}",
                        match_id, next_id, requester_username_esc);
                    send_notification(m->creator_id, ACTION_JOIN_REQUEST, notif_payload);

                    /* FIX: mancava un avviso a chi e' stato promosso dalla
                       coda. Il suo client aveva mostrato un popup "il
                       creatore sta valutando un'altra richiesta" quando era
                       finito in coda, ma non aveva modo di sapere quando
                       quella richiesta davanti si liberava, e quindi quel
                       popup restava aperto per sempre (bug segnalato
                       dall'utente). */
                    char promoted_payload[100];
                    snprintf(promoted_payload, sizeof(promoted_payload), "{\"match_id\":\"%s\"}", match_id);
                    send_notification(next_id, ACTION_JOIN_REQUEST_PROMOTED, promoted_payload);
                } else {
                    m->state = WAITING_FOR_OPPONENT;
                }

                printf("Giocatore %d rifiutato dalla partita %s\n", rejected_id, match_id);
            }

            sync_unlock_matches();
        }
        else if (strcmp(msg.action, ACTION_CANCEL_JOIN) == 0) {
            /* Chi ha fatto la richiesta (in decisione o in coda) la annulla
               di propria iniziativa (bottone "Annulla richiesta"). */
            char match_id[36] = {0};
            sscanf(msg.payload, "{\"match_id\":\"%35[^\"]\"}", match_id);

            sync_lock_matches();

            Match* m = find_match_by_id(match_id);
            if (m != NULL) {
                if (m->pending_requester_id == player_id) {
                    /* FIX: il creatore potrebbe avere gia' il dialog
                       si/no aperto su questa richiesta proprio ora. Se
                       risponde DOPO che l'abbiamo annullata, vogliamo poter
                       dirgli chi ha ritirato invece del generico "nessuna
                       richiesta". Se pero' c'e' subito una nuova richiesta
                       da proporgli (promozione dalla coda), quella nuova
                       notifica prende il sopravvento e non serve piu'
                       menzionare il ritiro di prima. */
                    m->withdrawn_requester_id = player_id;
                    m->pending_requester_id = -1;
                    int next_id = dequeue_join_request(m);
                    if (next_id != -1) {
                        m->pending_requester_id = next_id;
                        m->withdrawn_requester_id = -1;
                        m->state = JOIN_PENDING;

                        char requester_username[64], requester_username_esc[128];
                        get_username(next_id, requester_username, sizeof(requester_username));
                        json_escape(requester_username, requester_username_esc, sizeof(requester_username_esc));

                        char notif_payload[300];
                        snprintf(notif_payload, sizeof(notif_payload),
                            "{\"match_id\":\"%s\",\"requester_id\":%d,\"requester_username\":\"%s\"}",
                            match_id, next_id, requester_username_esc);
                        send_notification(m->creator_id, ACTION_JOIN_REQUEST, notif_payload);

                        /* FIX: come nel rifiuto, anche qui bisogna avvisare chi
                           e' stato promosso dalla coda, altrimenti il suo popup
                           "in coda" resta aperto per sempre. */
                        char promoted_payload[100];
                        snprintf(promoted_payload, sizeof(promoted_payload), "{\"match_id\":\"%s\"}", match_id);
                        send_notification(next_id, ACTION_JOIN_REQUEST_PROMOTED, promoted_payload);
                    } else {
                        m->state = WAITING_FOR_OPPONENT;
                    }
                } else {
                    remove_from_join_queue(m, player_id);
                }
            }

            strcpy(response.action, ACTION_CANCEL_JOIN);
            strcpy(response.payload, "{\"status\":\"ok\"}");
            send_response(player_id, &response);

            sync_unlock_matches();
        }
        else if (strcmp(msg.action, ACTION_MOVE) == 0) {
            char match_id[36] = {0};
            int column = -1;
            sscanf(msg.payload, "{\"match_id\":\"%35[^\"]\",\"column\":%d}", match_id, &column);

            sync_lock_matches();

            Match* m = find_match_by_id(match_id);
            if (m == NULL) {
                strcpy(response.error, "Match not found");
                strcpy(response.action, ACTION_MOVE);
                send_response(player_id, &response);
            } else {
                MoveResult mr = apply_match_move(m, player_id, column);

                if (mr.result < 0) {
                    /* Mossa rifiutata: avviso solo a chi ha provato a giocarla */
                    if (mr.result == -1) strcpy(response.error, "Not your turn");
                    else if (mr.result == -2) strcpy(response.error, "Invalid move");
                    else strcpy(response.error, "Match not active");

                    strcpy(response.action, ACTION_MOVE);
                    send_response(player_id, &response);
                } else {
                    /* FIX principale: prima la mossa veniva comunicata solo a chi
                       l'aveva giocata. Ora costruiamo lo stato aggiornato (board,
                       ultima mossa, turno, esito) e lo mandiamo in NOTIFICATION
                       a ENTRAMBI i giocatori, cosi' entrambe le board restano
                       sincronizzate ad ogni mossa. */
                    char board_json[256];
                    pthread_mutex_lock(&m->mutex);
                    build_board_json(m->board, board_json, sizeof(board_json));
                    pthread_mutex_unlock(&m->mutex);

                    const char* status_str = (mr.result == 1) ? "win" : (mr.result == 2) ? "draw" : "ok";
                    /* FIX: "0" come sentinella di "nessun vincitore" collideva col
                       vero player_id del primo client che si connette (gli id
                       partono da 0). Quando player 0 vinceva, il client riceveva
                       winner_id=0 e lo leggeva come "nessun vincitore" invece che
                       come "ha vinto io" -> entrambi i client mostravano "Hai
                       perso". -1 non e' mai un player_id valido, quindi va bene
                       come sentinella. */
                    int winner_id = (mr.result == 1) ? player_id : -1;

                    /* FIX: per il rigioco fedele alla traccia, il server
                       deve ricordarsi CHI ha vinto (o se e' un pareggio)
                       anche dopo che lo stato passa a FINISHED, per poter
                       distinguere piu' avanti (in ACTION_REMATCH) tra "solo
                       il vincitore puo' aprire una nuova sessione" e
                       "pareggio, serve il consenso di entrambi". Scrittura
                       innocua anche sulle mosse non conclusive (mr.result
                       0): in quel caso winner_id vale sempre -1, ma non
                       verra' mai letto finche' lo stato non e' FINISHED. */
                    if (mr.result == 1 || mr.result == 2) {
                        pthread_mutex_lock(&m->mutex);
                        m->winner_id = winner_id;
                        pthread_mutex_unlock(&m->mutex);

                        /* FIX: ora che le partite ACTIVE restano visibili in
                           lobby come "IN CORSO" (richiesto dall'utente),
                           quando si concludono va segnalato anche questo a
                           chi le stava vedendo, altrimenti resterebbero
                           marcate "IN CORSO" fino al prossimo refresh
                           periodico (fino a 3s dopo). */
                        broadcast_match_unavailable(match_id, m->player1_id, m->player2_id);
                    }

                    char notif_payload[400];
                    snprintf(notif_payload, sizeof(notif_payload),
                        "{\"match_id\":\"%s\",\"row\":%d,\"col\":%d,\"symbol\":%d,\"board\":%s,\"current_turn\":%d,\"status\":\"%s\",\"winner_id\":%d}",
                        match_id, mr.row, column, mr.symbol, board_json, mr.next_turn, status_str, winner_id);

                    send_notification(m->player1_id, ACTION_MOVE, notif_payload);
                    send_notification(m->player2_id, ACTION_MOVE, notif_payload);
                }
            }

            sync_unlock_matches();
        }
        else if (strcmp(msg.action, ACTION_REMATCH) == 0) {
            /* FIX (fedelta' alla traccia): prima QUALSIASI dei due giocatori
               poteva far ripartire la partita unilateralmente (bastava un
               solo "Si"), con la stessa coppia riunita automaticamente. La
               traccia invece distingue due casi:
               - vittoria/sconfitta: SOLO il vincitore puo' decidere di
                 aprire una nuova sessione, diventandone proprietario; il
                 perdente non ha voce in capitolo (e' gia' stato "obbligato
                 a lasciare la partita" altrove, vedi ACTION_QUIT_MATCH). La
                 nuova sessione torna in stato di attesa ed e' aperta a
                 chiunque in lobby, non necessariamente allo stesso
                 avversario di prima.
               - pareggio: serve il consenso di ENTRAMBI i giocatori (voto
                 congiunto). Il primo "si" viene solo registrato; la
                 partita riparte (con la stessa coppia, che si e' appena
                 accordata esplicitamente) solo quando anche il secondo
                 giocatore vota "si". */
            char match_id[36] = {0};
            sscanf(msg.payload, "{\"match_id\":\"%35[^\"]\"}", match_id);

            sync_lock_matches();

            Match* m = find_match_by_id(match_id);
            strcpy(response.action, ACTION_REMATCH);

            if (m == NULL || !is_player_in_match(m, player_id) || m->state != FINISHED) {
                strcpy(response.error, "Cannot start rematch");
                send_response(player_id, &response);
            } else if (m->winner_id != -1) {
                /* Vittoria/sconfitta */
                if (player_id != m->winner_id) {
                    strcpy(response.error, "Solo il vincitore puo' avviare una nuova sessione");
                    send_response(player_id, &response);
                } else {
                    open_new_session_as_owner(m, player_id);

                    char board_json[256];
                    build_board_json(m->board, board_json, sizeof(board_json));

                    snprintf(response.payload, sizeof(response.payload),
                        "{\"status\":\"new_session\",\"match_id\":\"%s\",\"player_number\":1,\"board\":%s,\"current_turn\":%d}",
                        match_id, board_json, m->current_turn);
                    send_response(player_id, &response);

                    printf("Giocatore %d ha aperto una nuova sessione (vincitore) su %s\n", player_id, match_id);
                }
            } else {
                /* Pareggio: voto congiunto */
                int is_p1 = (player_id == m->player1_id);
                if (is_p1) m->rematch_vote_p1 = 1; else m->rematch_vote_p2 = 1;
                int other_vote = is_p1 ? m->rematch_vote_p2 : m->rematch_vote_p1;

                if (other_vote == 1) {
                    /* Anche l'altro aveva gia' votato si: entrambi d'accordo,
                       si riparte insieme (stessa coppia di giocatori). */
                    reset_match_for_rematch(m);

                    char board_json[256];
                    build_board_json(m->board, board_json, sizeof(board_json));

                    char username_p1[64], username_p2[64];
                    get_username(m->player1_id, username_p1, sizeof(username_p1));
                    get_username(m->player2_id, username_p2, sizeof(username_p2));
                    char username_p1_esc[128], username_p2_esc[128];
                    json_escape(username_p1, username_p1_esc, sizeof(username_p1_esc));
                    json_escape(username_p2, username_p2_esc, sizeof(username_p2_esc));

                    strcpy(response.payload, "{\"status\":\"ok\"}");
                    send_response(player_id, &response);

                    char notif_p1[640], notif_p2[640];
                    snprintf(notif_p1, sizeof(notif_p1),
                        "{\"match_id\":\"%s\",\"player_number\":1,\"opponent_id\":%d,\"opponent_username\":\"%s\",\"board\":%s,\"current_turn\":%d}",
                        match_id, m->player2_id, username_p2_esc, board_json, m->current_turn);
                    snprintf(notif_p2, sizeof(notif_p2),
                        "{\"match_id\":\"%s\",\"player_number\":2,\"opponent_id\":%d,\"opponent_username\":\"%s\",\"board\":%s,\"current_turn\":%d}",
                        match_id, m->player1_id, username_p1_esc, board_json, m->current_turn);

                    send_notification(m->player1_id, ACTION_MATCH_STARTED, notif_p1);
                    send_notification(m->player2_id, ACTION_MATCH_STARTED, notif_p2);

                    printf("Rivincita (pareggio, accordo di entrambi) avviata per partita %s\n", match_id);
                } else {
                    strcpy(response.payload, "{\"status\":\"waiting_for_opponent_vote\"}");
                    send_response(player_id, &response);

                    printf("Giocatore %d ha votato si per la rivincita (pareggio) su %s, in attesa dell'altro\n", player_id, match_id);
                }
            }

            sync_unlock_matches();
        }
        else if (strcmp(msg.action, ACTION_QUIT_MATCH) == 0) {
            /* FIX: ACTION_QUIT_MATCH gestiva solo il caso "partita gia'
               finita" (rifiuto rivincita). Ma il bottone "Torna alla Lobby"
               nel client e' sempre visibile, anche a partita ATTIVA: se un
               giocatore lo premeva a meta' partita, usciva in locale senza
               che il server o l'avversario venissero avvisati -> l'altro
               restava a fissare la board in attesa di una mossa che non
               sarebbe mai arrivata, esattamente come nel bug della
               disconnessione che avevamo gia' risolto. Ora il comportamento
               dipende dallo stato della partita. */
            char match_id[36] = {0};
            sscanf(msg.payload, "{\"match_id\":\"%35[^\"]\"}", match_id);

            sync_lock_matches();
            Match* m = find_match_by_id(match_id);
            if (m != NULL && is_player_in_match(m, player_id)) {
                int opponent_id = get_opponent_id(m, player_id);

                if (m->state == ACTIVE) {
                    /* Uscita volontaria da una partita IN CORSO: e' un
                       abbandono a tutti gli effetti, l'avversario vince a
                       tavolino. Riusiamo abandon_match() (gia' scritta per la
                       disconnessione) e notifichiamo con ACTION_OPPONENT_LEFT,
                       cosi' il client gestisce i due casi allo stesso modo. */
                    abandon_match(m, player_id);
                    if (opponent_id != -1) {
                        char payload[200];
                        snprintf(payload, sizeof(payload),
                            "{\"match_id\":\"%s\",\"winner_id\":%d,\"reason\":\"left\"}",
                            match_id, opponent_id);
                        send_notification(opponent_id, ACTION_OPPONENT_LEFT, payload);
                    }
                    /* Anche qui: la partita era visibile come "IN CORSO" a
                       chi non ci giocava, ora va tolta dalla loro lista. */
                    broadcast_match_unavailable(match_id, player_id, opponent_id);
                } else if (m->state == WAITING_FOR_OPPONENT || m->state == JOIN_PENDING) {
                    /* FIX: prima si assumeva che nessuno fosse mai in attesa
                       di una decisione a questo punto. Ma il creatore puo'
                       tornare alla lobby anche mentre c'e' una richiesta di
                       partecipazione in decisione o altre in coda: vanno
                       avvisate, altrimenti resterebbero bloccate per sempre
                       nella schermata di attesa. */
                    if (m->pending_requester_id != -1) {
                        char req_payload[200];
                        snprintf(req_payload, sizeof(req_payload),
                            "{\"match_id\":\"%s\",\"reason\":\"closed\"}", match_id);
                        send_notification(m->pending_requester_id, ACTION_JOIN_REQUEST_OBSOLETE, req_payload);
                    }
                    for (int qi = 0; qi < m->join_queue_count; qi++) {
                        char req_payload[200];
                        snprintf(req_payload, sizeof(req_payload),
                            "{\"match_id\":\"%s\",\"reason\":\"closed\"}", match_id);
                        send_notification(m->join_queue[qi], ACTION_JOIN_REQUEST_OBSOLETE, req_payload);
                    }
                    /* segniamo la partita come abbandonata cosi' sparisce
                       dalla lista (list_matches mostra solo le partite
                       WAITING_FOR_OPPONENT/JOIN_PENDING). */
                    m->state = ABANDONED;
                } else if (m->state == FINISHED) {
                    /* FIX (fedelta' alla traccia): "il perdente e' obbligato
                       a lasciare la partita" - la sua uscita e' un passaggio
                       AUTOMATICO e previsto del flusso, non deve avvisare
                       ne' interrompere l'eventuale decisione del vincitore
                       su una nuova sessione (che e' del tutto indipendente).
                       Se invece a uscire e' chi decide (il vincitore in
                       vittoria/sconfitta, uno dei due in caso di pareggio),
                       quello si' e' un rifiuto esplicito: l'eventuale
                       avversario ancora in attesa va avvisato. */
                    if (m->winner_id != -1 && player_id != m->winner_id) {
                        /* Il perdente lascia: nessuna notifica, nessun
                           cambio di stato. */
                    } else {
                        if (opponent_id != -1) {
                            char payload[200];
                            snprintf(payload, sizeof(payload), "{\"match_id\":\"%s\"}", match_id);
                            send_notification(opponent_id, ACTION_MATCH_CLOSED, payload);
                        }
                        m->state = ABANDONED;
                    }
                }
                /* ABANDONED: gia' chiusa da qualcun altro, nessuna azione. */
            }
            sync_unlock_matches();
        }
        else {
            strcpy(response.error, "Unknown action");
            send_response(player_id, &response);
        }
    }

    /* Il client si e' disconnesso: se era in una partita attiva, l'avversario vince
       per abbandono e viene avvisato. FIX: prima questo non succedeva mai, l'altro
       giocatore restava in attesa di una mossa che non sarebbe mai arrivata. */
    sync_lock_matches();
    for (int i = 0; i < matches_count; i++) {
        Match* m = matches[i];

        if (m->state == ACTIVE && is_player_in_match(m, player_id)) {
            int opponent_id = get_opponent_id(m, player_id);
            abandon_match(m, player_id);

            if (opponent_id != -1) {
                char payload[200];
                snprintf(payload, sizeof(payload),
                    "{\"match_id\":\"%s\",\"winner_id\":%d,\"reason\":\"disconnect\"}",
                    m->match_id, opponent_id);
                send_notification(opponent_id, ACTION_OPPONENT_LEFT, payload);
            }
            broadcast_match_unavailable(m->match_id, player_id, opponent_id);
        }
        /* FIX: mancava del tutto la gestione della disconnessione durante la
           fase di richiesta di partecipazione (prima non esisteva questa
           fase). Se a disconnettersi e' il creatore, chi era in decisione o
           in coda va avvisato invece di restare bloccato per sempre nella
           schermata di attesa; se e' proprio il richiedente in decisione,
           si promuove il prossimo in coda; se era solo in coda, lo si toglie. */
        else if ((m->state == WAITING_FOR_OPPONENT || m->state == JOIN_PENDING) && m->creator_id == player_id) {
            if (m->pending_requester_id != -1) {
                char req_payload[200];
                snprintf(req_payload, sizeof(req_payload),
                    "{\"match_id\":\"%s\",\"reason\":\"closed\"}", m->match_id);
                send_notification(m->pending_requester_id, ACTION_JOIN_REQUEST_OBSOLETE, req_payload);
            }
            for (int qi = 0; qi < m->join_queue_count; qi++) {
                char req_payload[200];
                snprintf(req_payload, sizeof(req_payload),
                    "{\"match_id\":\"%s\",\"reason\":\"closed\"}", m->match_id);
                send_notification(m->join_queue[qi], ACTION_JOIN_REQUEST_OBSOLETE, req_payload);
            }
            m->state = ABANDONED;
        }
        else if (m->state == JOIN_PENDING && m->pending_requester_id == player_id) {
            m->pending_requester_id = -1;
            int next_id = dequeue_join_request(m);
            if (next_id != -1) {
                m->pending_requester_id = next_id;
                m->withdrawn_requester_id = -1;

                char requester_username[64], requester_username_esc[128];
                get_username(next_id, requester_username, sizeof(requester_username));
                json_escape(requester_username, requester_username_esc, sizeof(requester_username_esc));

                char notif_payload[300];
                snprintf(notif_payload, sizeof(notif_payload),
                    "{\"match_id\":\"%s\",\"requester_id\":%d,\"requester_username\":\"%s\"}",
                    m->match_id, next_id, requester_username_esc);
                send_notification(m->creator_id, ACTION_JOIN_REQUEST, notif_payload);

                /* FIX: stesso avviso di promozione anche quando la richiesta
                   davanti in coda scompare per disconnessione (non solo per
                   rifiuto/annullamento esplicito). */
                char promoted_payload[100];
                snprintf(promoted_payload, sizeof(promoted_payload), "{\"match_id\":\"%s\"}", m->match_id);
                send_notification(next_id, ACTION_JOIN_REQUEST_PROMOTED, promoted_payload);
            } else {
                m->state = WAITING_FOR_OPPONENT;
            }
        }
        else if (m->state == JOIN_PENDING) {
            remove_from_join_queue(m, player_id);
        }
        /* FIX: mancava la gestione della disconnessione di un giocatore da
           una partita gia' FINISHED (es. chiude il client senza cliccare
           nulla sul dialog di fine partita). Stessa logica di
           ACTION_QUIT_MATCH: se e' il perdente, nessuna azione (uscita
           automatica prevista); altrimenti (vincitore o pareggio) avvisiamo
           l'eventuale avversario ancora in attesa e chiudiamo. */
        else if (m->state == FINISHED && is_player_in_match(m, player_id)) {
            int opponent_id = get_opponent_id(m, player_id);
            if (m->winner_id != -1 && player_id != m->winner_id) {
                /* Il perdente si e' disconnesso: nessuna azione. */
            } else {
                if (opponent_id != -1) {
                    char payload[200];
                    snprintf(payload, sizeof(payload), "{\"match_id\":\"%s\"}", m->match_id);
                    send_notification(opponent_id, ACTION_MATCH_CLOSED, payload);
                }
                m->state = ABANDONED;
            }
        }
    }
    sync_unlock_matches();

    close_connection(client_socket);

    sync_lock_players();
    if (player_id < MAX_PLAYERS && players[player_id]) {
        players[player_id]->state = PLAYER_DISCONNECTED;
        players[player_id]->current_match_id[0] = '\0';
    }
    sync_unlock_players();

    printf("Thread client %d terminato\n", player_id);
    return NULL;
}

int main() {
    printf("=== FORZA 4 SERVER ===\n");

    srand((unsigned int)time(NULL));
    sync_init();

    int server_socket = create_server_socket(SERVER_PORT);
    if (server_socket == -1) {
        printf("Errore: impossibile creare il socket server\n");
        return 1;
    }

    printf("Server avviato su porta %d\n", SERVER_PORT);

    int player_id_counter = 0;

    while (1) {
        printf("\nIn attesa di connessioni...\n");

        int client_socket = accept_client_connection(server_socket);
        if (client_socket == -1) {
            printf("Errore nell'accettare la connessione\n");
            continue;
        }

        sync_lock_players();

        if (players_count >= MAX_PLAYERS) {
            printf("Errore: troppi client connessi\n");
            close_connection(client_socket);
            sync_unlock_players();
            continue;
        }

        /* FIX (robustezza, scoperto durante una revisione approfondita):
           malloc() NON azzera la memoria. Player.username e' un char[64]
           riempito solo in parte da strcpy("Unknown") qui e poi da
           strncpy(..., n) al momento della registrazione: strncpy scrive
           SEMPRE esattamente n byte (n = sizeof(username)-1) ma non tocca
           MAI l'ultimo byte dell'array (l'indice sizeof(username)-1), che
           quindi restava garbage non inizializzata proveniente da malloc()
           invece di un sicuro '\0' - stesso problema per current_match_id.
           In pratica su Linux spesso "funziona" comunque perche' il kernel
           tende a restituire pagine fresche gia' azzerate, ma non e'
           garantito dallo standard C ed e' fragile: se quel byte finale
           risultasse non-zero, ogni lettura della stringa (strlen, printf,
           json_escape...) andrebbe oltre i limiti dei 64 byte dell'array
           cercando un terminatore che non c'e', leggendo memoria adiacente
           non sua. calloc() azzera tutta la struct in partenza, eliminando
           del tutto il problema per username, current_match_id ed eventuali
           campi futuri, senza cambiare alcun comportamento per i campi gia'
           impostati esplicitamente qui sotto. */
        Player* new_player = (Player*)calloc(1, sizeof(Player));
        new_player->player_id = player_id_counter++;
        new_player->socket_fd = client_socket;
        new_player->state = PLAYER_CONNECTED;
        new_player->last_activity = time(NULL);
        new_player->current_match_id[0] = '\0';
        strcpy(new_player->username, "Unknown");
        pthread_mutex_init(&new_player->send_mutex, NULL);

        players[players_count] = new_player;
        int player_id = new_player->player_id;
        players_count++;

        sync_unlock_players();

        pthread_t thread_id;
        ClientThreadArgs* args = (ClientThreadArgs*)malloc(sizeof(ClientThreadArgs));
        args->client_socket = client_socket;
        args->player_id = player_id;

        pthread_create(&thread_id, NULL, client_thread_handler, args);
        pthread_detach(thread_id);

        printf("Nuovo client connesso (ID: %d)\n", player_id);
    }

    return 0;
}
