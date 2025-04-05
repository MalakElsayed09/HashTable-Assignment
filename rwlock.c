#include "rwlock.h"

// Initialize rwlock
void rwlock_init(rwlock_t *lock) {
    pthread_mutex_init(&lock->mutex, NULL);
    pthread_cond_init(&lock->readers_proceed, NULL);
    pthread_cond_init(&lock->writers_proceed, NULL);
    lock->readers = 0;
    lock->writers = 0;
    lock->write_waiting = 0;
}

// Acquire read lock
void rwlock_acquire_readlock(rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);

    // Wait if a writer is active or waiting
    while (lock->writers > 0 || lock->write_waiting > 0)
        pthread_cond_wait(&lock->readers_proceed, &lock->mutex);

    // Reader can proceed
    lock->readers++;

    pthread_mutex_unlock(&lock->mutex);
}

// Release the read lock
void rwlock_release_readlock(rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);

    lock->readers--;

    // If no readers left, signal one writer
    if (lock->readers == 0)
        pthread_cond_signal(&lock->writers_proceed);

    pthread_mutex_unlock(&lock->mutex);
}

// Acquire the write lock
void rwlock_acquire_writelock(rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);

    // Indicate intent to write
    lock->write_waiting++;

    // Wait while readers or writers are active
    while (lock->readers > 0 || lock->writers > 0)
        pthread_cond_wait(&lock->writers_proceed, &lock->mutex);

    // Writer can proceed
    lock->write_waiting--;
    lock->writers++;

    pthread_mutex_unlock(&lock->mutex);
}

// Release write lock
void rwlock_release_writelock(rwlock_t *lock) {
    pthread_mutex_lock(&lock->mutex);

    lock->writers--;

    if (lock->write_waiting > 0) {
        // Let the next writer go
        pthread_cond_signal(&lock->writers_proceed);
    } else {
        // No waiting writers — wake up all readers
        pthread_cond_broadcast(&lock->readers_proceed);
    }

    pthread_mutex_unlock(&lock->mutex);
}

// Destroys the lock
void rwlock_destroy(rwlock_t *lock) {
    pthread_mutex_destroy(&lock->mutex);
    pthread_cond_destroy(&lock->readers_proceed);
    pthread_cond_destroy(&lock->writers_proceed);
}
