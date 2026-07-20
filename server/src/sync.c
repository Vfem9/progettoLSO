#include <pthread.h>
#include "../include/sync.h"

// Variabile globale: contiene tutti i mutex del programma
GlobalSync g_sync;

// Inizializza tutti i mutex
void sync_init() {
    pthread_mutex_init(&g_sync.matches_mutex, NULL);
    pthread_mutex_init(&g_sync.players_mutex, NULL);
}

// Distrugge tutti i mutex (quando il programma termina)
void sync_destroy() {
    pthread_mutex_destroy(&g_sync.matches_mutex);
    pthread_mutex_destroy(&g_sync.players_mutex);
}

// Acquisisce il lucchetto sulla lista partite (blocca altri thread)
void sync_lock_matches() {
    pthread_mutex_lock(&g_sync.matches_mutex);
}

// Rilascia il lucchetto sulla lista partite (permette ad altri thread di accedere)
void sync_unlock_matches() {
    pthread_mutex_unlock(&g_sync.matches_mutex);
}

// Acquisisce il lucchetto sulla lista giocatori
void sync_lock_players() {
    pthread_mutex_lock(&g_sync.players_mutex);
}

// Rilascia il lucchetto sulla lista giocatori
void sync_unlock_players() {
    pthread_mutex_unlock(&g_sync.players_mutex);
}