#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "../include/game.h"
#include "../include/match.h"
#include "../include/player.h"
#include "../include/network.h"
#include "../include/protocol.h"
#include "../include/sync.h"

#define MAX_MATCHES 100
#define MAX_PLAYERS 100
#define SERVER_PORT 5000
#define TIMEOUT_SECONDS 30

Match* matches[MAX_MATCHES];
int matches_count = 0;

Player* players[MAX_PLAYERS];
int players_count = 0;

typedef struct {
    int client_socket;
    int player_id;
} ClientThreadArgs;

void* client_thread_handler(void* args) {
    ClientThreadArgs* thread_args = (ClientThreadArgs*)args;
    int client_socket = thread_args->client_socket;
    int player_id = thread_args->player_id;
    free(thread_args);
    
    time_t last_activity = time(NULL);
    
    printf("Thread client %d avviato\n", player_id);
    
    while (1) {
        time_t current_time = time(NULL);
        if (current_time - last_activity > TIMEOUT_SECONDS) {
            printf("Timeout per giocatore %d\n", player_id);
            break;
        }
        
        Message msg = receive_message(client_socket);
        
        if (msg.error[0] != '\0') {
            printf("Errore ricezione messaggio da giocatore %d\n", player_id);
            break;
        }
        
        last_activity = time(NULL);
        
        printf("Ricevuto: type=%s, action=%s da giocatore %d\n", msg.type, msg.action, player_id);
        
        Message response;
        memset(&response, 0, sizeof(Message));
        strcpy(response.type, MSG_TYPE_RESPONSE);
        strcpy(response.msg_id, msg.msg_id);
        
        if (strcmp(msg.action, ACTION_REGISTER) == 0) {
            char username[64] = {0};
            sscanf(msg.payload, "{\"username\":\"%63[^\"]\"}",  username);
            
            sync_lock_players();
            strcpy(players[player_id]->username, username);
            players[player_id]->state = PLAYER_WAITING;
            sync_unlock_players();
            
            strcpy(response.action, ACTION_REGISTER);
            strcpy(response.payload, "{\"player_id\":1,\"status\":\"ok\"}");
            
            printf("Giocatore registrato: %s (ID: %d)\n", username, player_id);
        }
        else if (strcmp(msg.action, ACTION_CREATE_MATCH) == 0) {
            sync_lock_matches();
            
            if (matches_count >= MAX_MATCHES) {
                strcpy(response.error, "Too many matches");
                sync_unlock_matches();
            } else {
                char match_id[36];
                snprintf(match_id, sizeof(match_id), "m_%d_%ld", player_id, time(NULL));
                
                Match* new_match = create_match(player_id, match_id);
                matches[matches_count++] = new_match;
                
                snprintf(response.payload, sizeof(response.payload), 
                    "{\"match_id\":\"%s\",\"status\":\"ok\"}", match_id);
                strcpy(response.action, ACTION_CREATE_MATCH);
                
                sync_unlock_matches();
                
                printf("Partita creata: %s\n", match_id);
            }
        }
        else if (strcmp(msg.action, ACTION_LIST_MATCHES) == 0) {
            sync_lock_matches();
            
            char payload[1024] = "{\"matches\":[";
            int first = 1;
            
            for (int i = 0; i < matches_count; i++) {
                if (matches[i]->state == WAITING_FOR_OPPONENT) {
                    if (!first) strcat(payload, ",");
                    
                    char match_info[256];
                    snprintf(match_info, sizeof(match_info),
                        "{\"match_id\":\"%s\",\"creator\":%d,\"status\":\"waiting\"}",
                        matches[i]->match_id, matches[i]->creator_id);
                    strcat(payload, match_info);
                    first = 0;
                }
            }
            
            strcat(payload, "]}");
            strcpy(response.action, ACTION_LIST_MATCHES);
            strcpy(response.payload, payload);
            
            sync_unlock_matches();
            
            printf("Lista partite inviata a giocatore %d\n", player_id);
        }
        else {
            strcpy(response.error, "Unknown action");
        }
        
        send_message(client_socket, &response);
    }
    
    close_connection(client_socket);
    
    sync_lock_players();
    players[player_id]->state = PLAYER_DISCONNECTED;
    sync_unlock_players();
    
    printf("Thread client %d terminato\n", player_id);
    return NULL;
}

int main() {
    printf("=== FORZA 4 SERVER ===\n");
    
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
        
        Player* new_player = (Player*)malloc(sizeof(Player));
        new_player->player_id = player_id_counter++;
        new_player->socket_fd = client_socket;
        new_player->state = PLAYER_CONNECTED;
        new_player->last_activity = time(NULL);
        new_player->current_match_id = -1;
        strcpy(new_player->username, "Unknown");
        
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