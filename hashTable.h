#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include "rwlock.h"

// Hash table record structure as specified in the assignment
typedef struct hash_struct
{
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

// Hash table with synchronization primitives
typedef struct {
    hashRecord *head;                // Single linked list head
    pthread_rwlock_t rwlock;         // Reader-writer lock
    pthread_mutex_t cv_mutex;        // Mutex for condition variables
    pthread_cond_t insert_complete;  // CV for insert-delete synchronization
    int active_inserts;              // Track ongoing inserts
    bool inserts_complete;           // Flag to mark all inserts as complete
    int lock_acquisitions;           // Track lock acquisitions
    int lock_releases;               // Track lock releases
    FILE *output_file;               // Output file handle
} HashTable;

// Initialize the hash table
HashTable* init_table(const char* output_filename);

// Jenkins one-at-a-time hash function
uint32_t jenkins_one_at_a_time_hash(const char *key);

// Insert a record into the hash table
void insert(HashTable *table, const char *name, uint32_t salary);

// Delete a record from the hash table
void delete(HashTable *table, const char *name);

// Search for a record in the hash table
hashRecord* search(HashTable *table, const char *name);

// Print the entire hash table (sorted by hash)
void print_table(HashTable *table);

// Print a single record
void print_record(FILE *file, hashRecord *record);

// Free all resources used by the hash table
void free_table(HashTable *table);

// Print final summary
void print_summary(HashTable *table);

// Get current timestamp in microseconds
uint64_t get_timestamp_us();

// Log with timestamp
void log_with_timestamp(FILE *file, const char *format, ...);

#endif // HASHTABLE_H
