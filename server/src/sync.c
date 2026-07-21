#include <pthread.h>
#include "../include/sync.h"

GlobalSync g_sync;

void sync_init() {
    pthread_mutex_init(&g_sync.matches_mutex, NULL);
    pthread_mutex_init(&g_sync.players_mutex, NULL);
}

void sync_destroy() {
    pthread_mutex_destroy(&g_sync.matches_mutex);
    pthread_mutex_destroy(&g_sync.players_mutex);
}

void sync_lock_matches() {
    pthread_mutex_lock(&g_sync.matches_mutex);
}

void sync_unlock_matches() {
    pthread_mutex_unlock(&g_sync.matches_mutex);
}

void sync_lock_players() {
    pthread_mutex_lock(&g_sync.players_mutex);
}

void sync_unlock_players() {
    pthread_mutex_unlock(&g_sync.players_mutex);
}
