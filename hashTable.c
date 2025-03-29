#include "hashtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

// Initialize the hash table
HashTable* init_table(const char* output_filename) {
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    if (!table) {
        perror("Failed to allocate memory for hash table");
        exit(EXIT_FAILURE);
    }

    // Initialize hash table
    table->head = NULL;
    table->lock_acquisitions = 0;
    table->lock_releases = 0;
    table->active_inserts = 0;
    table->inserts_complete = false;

    // Initialize reader-writer lock
    if (pthread_rwlock_init(&table->rwlock, NULL) != 0) {
        perror("Failed to initialize rwlock");
        free(table);
        exit(EXIT_FAILURE);
    }

    // Initialize mutex and condition variable
    if (pthread_mutex_init(&table->cv_mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        pthread_rwlock_destroy(&table->rwlock);
        free(table);
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&table->insert_complete, NULL) != 0) {
        perror("Failed to initialize condition variable");
        pthread_mutex_destroy(&table->cv_mutex);
        pthread_rwlock_destroy(&table->rwlock);
        free(table);
        exit(EXIT_FAILURE);
    }

    // Open output file
    table->output_file = fopen(output_filename, "w");
    if (!table->output_file) {
        perror("Failed to open output file");
        pthread_cond_destroy(&table->insert_complete);
        pthread_mutex_destroy(&table->cv_mutex);
        pthread_rwlock_destroy(&table->rwlock);
        free(table);
        exit(EXIT_FAILURE);
    }

    return table;
}

// Get current timestamp for logging
static void log_timestamp(FILE *file) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(file, "%ld", ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
}

// Jenkins' one-at-a-time hash function
uint32_t jenkins_one_at_a_time_hash(const char *key) {
    uint32_t hash = 0;
    size_t i = 0;
    
    while (key[i] != '\0') {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    
    return hash;
}

// Insert a record into the hash table
void insert(HashTable *table, const char *name, uint32_t salary) {
    uint32_t hash = jenkins_one_at_a_time_hash(name);
    
    // Log the insert command
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",INSERT,%s,%u\n", name, salary);
    
    // Track active inserts for delete synchronization
    pthread_mutex_lock(&table->cv_mutex);
    table->active_inserts++;
    pthread_mutex_unlock(&table->cv_mutex);
    
    // Acquire write lock
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",WRITE LOCK ACQUIRED\n");
    pthread_rwlock_wrlock(&table->rwlock);
    table->lock_acquisitions++;
    
    // Search for existing record with the same hash
    hashRecord *current = table->head;
    hashRecord *prev = NULL;
    
    while (current && current->hash < hash) {
        prev = current;
        current = current->next;
    }
    
    // If hash exists, update the record
    if (current && current->hash == hash) {
        strncpy(current->name, name, sizeof(current->name) - 1);
        current->name[sizeof(current->name) - 1] = '\0'; // Ensure null termination
        current->salary = salary;
    } else {
        // Create a new record
        hashRecord *new_record = (hashRecord *)malloc(sizeof(hashRecord));
        if (!new_record) {
            perror("Failed to allocate memory for new record");
            pthread_rwlock_unlock(&table->rwlock);
            table->lock_releases++;
            log_timestamp(table->output_file);
            fprintf(table->output_file, ",WRITE LOCK RELEASED\n");
            return;
        }
        
        new_record->hash = hash;
        strncpy(new_record->name, name, sizeof(new_record->name) - 1);
        new_record->name[sizeof(new_record->name) - 1] = '\0'; // Ensure null termination
        new_record->salary = salary;
        
        // Insert in sorted order by hash
        if (!prev) {
            // Insert at the beginning
            new_record->next = table->head;
            table->head = new_record;
        } else {
            // Insert in the middle or end
            new_record->next = prev->next;
            prev->next = new_record;
        }
    }
    
    // Release write lock
    pthread_rwlock_unlock(&table->rwlock);
    table->lock_releases++;
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",WRITE LOCK RELEASED\n");
    
    // Update insert tracking
    pthread_mutex_lock(&table->cv_mutex);
    table->active_inserts--;
    if (table->active_inserts == 0) {
        table->inserts_complete = true;
        pthread_cond_broadcast(&table->insert_complete);
    }
    pthread_mutex_unlock(&table->cv_mutex);
}

// Search for a record in the hash table
hashRecord* search(HashTable *table, const char *name) {
    uint32_t hash = jenkins_one_at_a_time_hash(name);
    
    // Log the search command
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",SEARCH,%s,0\n", name);
    
    // Acquire read lock
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",READ LOCK ACQUIRED\n");
    pthread_rwlock_rdlock(&table->rwlock);
    table->lock_acquisitions++;
    
    // Search for the record
    hashRecord *current = table->head;
    hashRecord *result = NULL;
    
    while (current) {
        if (current->hash == hash) {
            result = current;
            break;
        }
        current = current->next;
    }
    
    // Create a copy of the record if found
    hashRecord *copy = NULL;
    if (result) {
        copy = malloc(sizeof(hashRecord));
        if (copy) {
            memcpy(copy, result, sizeof(hashRecord));
            copy->next = NULL; // Isolate the copy
        }
    }
    
    // Release read lock
    pthread_rwlock_unlock(&table->rwlock);
    table->lock_releases++;
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",READ LOCK RELEASED\n");
    
    // If found, print the record
    if (result) {
        print_record(table->output_file, result);
    } else {
        fprintf(table->output_file, "No Record Found\n");
    }
    
    return copy; // Return the isolated copy
}

// Delete a record from the hash table
void delete(HashTable *table, const char *name) {
    uint32_t hash = jenkins_one_at_a_time_hash(name);
    
    // Log the delete command
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",DELETE,%s,0\n", name);
    
    // Wait for all inserts to complete
    pthread_mutex_lock(&table->cv_mutex);
    if (!table->inserts_complete && table->active_inserts > 0) {
        log_timestamp(table->output_file);
        fprintf(table->output_file, ": WAITING ON INSERTS\n");
        pthread_cond_wait(&table->insert_complete, &table->cv_mutex);
        log_timestamp(table->output_file);
        fprintf(table->output_file, ": DELETE AWAKENED\n");
    }
    pthread_mutex_unlock(&table->cv_mutex);
    
    // Acquire write lock
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",WRITE LOCK ACQUIRED\n");
    pthread_rwlock_wrlock(&table->rwlock);
    table->lock_acquisitions++;
    
    // Search for the record and delete it
    hashRecord *current = table->head;
    hashRecord *prev = NULL;
    
    while (current && current->hash != hash) {
        prev = current;
        current = current->next;
    }
    
    // If found, delete the record
    if (current) {
        if (!prev) {
            // First element
            table->head = current->next;
        } else {
            // Middle or last element
            prev->next = current->next;
        }
        free(current);
    }
    
    // Release write lock
    pthread_rwlock_unlock(&table->rwlock);
    table->lock_releases++;
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",WRITE LOCK RELEASED\n");
}

// Print a single record
void print_record(FILE *file, hashRecord *record) {
    if (record) {
        fprintf(file, "%u,%s,%u\n", record->hash, record->name, record->salary);
    }
}

// Print the entire hash table (sorted by hash)
void print_table(HashTable *table) {
    // Log the print command
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",PRINT,0,0\n");
    
    // Acquire read lock
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",READ LOCK ACQUIRED\n");
    pthread_rwlock_rdlock(&table->rwlock);
    table->lock_acquisitions++;
    
    // Print all records
    hashRecord *current = table->head;
    while (current) {
        print_record(table->output_file, current);
        current = current->next;
    }
    
    // Release read lock
    pthread_rwlock_unlock(&table->rwlock);
    table->lock_releases++;
    log_timestamp(table->output_file);
    fprintf(table->output_file, ",READ LOCK RELEASED\n");
}

// Free all resources used by the hash table
void free_table(HashTable *table) {
    if (!table) return;
    
    // Print the final summary
    fprintf(table->output_file, "\nNumber of lock acquisitions: %d\n", table->lock_acquisitions);
    fprintf(table->output_file, "Number of lock releases: %d\n", table->lock_releases);
    
    // Print the final table (without locks)
    hashRecord *current = table->head;
    while (current) {
        print_record(table->output_file, current);
        hashRecord *next = current->next;
        free(current);
        current = next;
    }
    
    // Close the output file
    fclose(table->output_file);
    
    // Clean up synchronization primitives
    pthread_rwlock_destroy(&table->rwlock);
    pthread_mutex_destroy(&table->cv_mutex);
    pthread_cond_destroy(&table->insert_complete);
    
    // Free the table
    free(table);
}
