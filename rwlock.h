#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>

typedef struct{
    pthread_mutex_t mutex;
    pthread_cond_t readers_proceed;
    pthread_cond_t writers_proceed;
    int readers;
    int writers;
    int write_waiting;
}rwlock_t;



//initialize rwlock
void rwlock_init(rwlock_t *lock);

//acquire rwlock
void rwlock_acquire_readlock(rwlock_t *lock);

//release readlock
void rwlock_release_readlock(rwlock_t *lock);

//acquire writelock
void rwlock_acquire_writelock(rwlock_t *lock);

//release writelock
void rwlock_release_writelock(rwlock_t *lock);

//destroy rwlock
void rwlock_destroy(rwlock_t *lock);

#endif //RWLOCK_H
