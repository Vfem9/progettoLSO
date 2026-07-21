#ifndef SYNC_H
#define SYNC_H

#include <pthread.h>

typedef struct {
    pthread_mutex_t matches_mutex;
    pthread_mutex_t players_mutex;
} GlobalSync;

extern GlobalSync g_sync;

void sync_init();
void sync_destroy();
void sync_lock_matches();
void sync_unlock_matches();
void sync_lock_players();
void sync_unlock_players();

#endif
