#include "hashTable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/time.h>


// Get current timestamp in microseconds
uint64_t get_timestamp_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000000 + tv.tv_usec;
}

// Log with timestamp
void log_with_timestamp(FILE *file, const char *format, ...) {
    uint64_t timestamp = get_timestamp_us();
    fprintf(file, "%llu: ", (unsigned long long)timestamp);

    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
}

// Initialize hash table
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
    rwlock_init(&table->rwlock);

    // Initialize mutex and condition variable
    if (pthread_mutex_init(&table->cv_mutex, NULL) != 0) {
        perror("Failed to initialize mutex");
        rwlock_destroy(&table->rwlock);
        free(table);
        exit(EXIT_FAILURE);
    }

    if (pthread_cond_init(&table->insert_complete, NULL) != 0) {
        perror("Failed to initialize condition variable");
        pthread_mutex_destroy(&table->cv_mutex);
        rwlock_destroy(&table->rwlock);
        free(table);
        exit(EXIT_FAILURE);
    }

    // Open output file
    table->output_file = fopen(output_filename, "w");
    if (!table->output_file) {
        perror("Failed to open output file");
        pthread_cond_destroy(&table->insert_complete);
        pthread_mutex_destroy(&table->cv_mutex);
        rwlock_destroy(&table->rwlock);
        free(table);
        exit(EXIT_FAILURE);
    }

    return table;
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
    log_with_timestamp(table->output_file, "INSERT,%u,%s,%u\n", hash, name, salary);

    //Track active inserts for delete synchronization
    pthread_mutex_lock(&table->cv_mutex);
    table->active_inserts++;
    pthread_mutex_unlock(&table->cv_mutex);

    // Acquire write lock
    log_with_timestamp(table->output_file, "WRITE LOCK ACQUIRED\n");
    rwlock_acquire_writelock(&table->rwlock);
    table->lock_acquisitions++;

    // Search for existing record with the same hash
    hashRecord *current = table->head;
    hashRecord *prev = NULL;

    while (current && current->hash < hash) {
        prev = current;
        current = current->next;
    }

    // If hash exists, update the record
    if (current && current->hash == hash && strcmp(current->name, name) == 0) {
        // Record already exists, update it
        current->salary = salary;
        fprintf(table->output_file, "Insert failed: %s already exists\n", name);
    } else {
        // Create a new record
        hashRecord *new_record = (hashRecord *)malloc(sizeof(hashRecord));
        if (!new_record) {
            perror("Failed to allocate memory for new record");
            rwlock_release_writelock(&table->rwlock);
            table->lock_releases++;
            log_with_timestamp(table->output_file, "WRITE LOCK RELEASED\n");
            return;
        }

        new_record->hash = hash;
        strncpy(new_record->name, name, sizeof(new_record->name) - 1);
        new_record->name[sizeof(new_record->name) - 1] = '\0';
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

        fprintf(table->output_file, "Inserted: %s, %u\n", name, salary);
    }

    // Release write lock
    rwlock_release_writelock(&table->rwlock);
    table->lock_releases++;
    log_with_timestamp(table->output_file, "WRITE LOCK RELEASED\n");

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

    //Log the search command
    log_with_timestamp(table->output_file, "SEARCH: %s\n", name);

    // Acquire read lock
    log_with_timestamp(table->output_file, "READ LOCK ACQUIRED\n");
    rwlock_acquire_readlock(&table->rwlock);
    table->lock_acquisitions++;

    // Search for the record
    hashRecord *current = table->head;
    hashRecord *result = NULL;

    while (current) {
        if (current->hash == hash && strcmp(current->name, name) == 0) {
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
            
            // Print the found record
            fprintf(table->output_file, "Found: %s, %u\n", result->name, result->salary);
        }
    } else {
        // Print "Not Found" if not found
        fprintf(table->output_file, "Not Found: %s\n", name);
    }

    // Release read lock
    rwlock_release_readlock(&table->rwlock);
    table->lock_releases++;
    log_with_timestamp(table->output_file, "READ LOCK RELEASED\n");

    return copy; // Return the isolated copy (caller is responsible for freeing)
}

void delete(HashTable *table, const char *name) {
    uint32_t hash = jenkins_one_at_a_time_hash(name);

    // Log the delete command
    log_with_timestamp(table->output_file, "DELETE,%s\n", name);

    // Wait for active inserts to finish
    pthread_mutex_lock(&table->cv_mutex);
    log_with_timestamp(table->output_file, "WAITING ON INSERTS\n");
    while (!table->inserts_complete && table->active_inserts > 0) {
        pthread_cond_wait(&table->insert_complete, &table->cv_mutex);
    }
    log_with_timestamp(table->output_file, "DELETE AWAKENED\n");
    pthread_mutex_unlock(&table->cv_mutex);

    // Acquire write lock
    log_with_timestamp(table->output_file, "WRITE LOCK ACQUIRED\n");
    rwlock_acquire_writelock(&table->rwlock);
    table->lock_acquisitions++;

    // Search for record to delete
    hashRecord *current = table->head;
    hashRecord *prev = NULL;
    bool found = false;

    while (current) {
        if (current->hash == hash && strcmp(current->name, name) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                table->head = current->next;
            }
            free(current);
            found = true;
            fprintf(table->output_file, "Deleted: %s\n", name);
            break;
        }
        prev = current;
        current = current->next;
    }

    if (!found) {
        fprintf(table->output_file, "Delete failed: %s not found\n", name);
    }

    // Release write lock
    rwlock_release_writelock(&table->rwlock);
    table->lock_releases++;
    log_with_timestamp(table->output_file, "WRITE LOCK RELEASED\n");
}

// Print the entire hash table (sorted by hash)
void print_table(HashTable *table) {.
    // Acquire read lock
    log_with_timestamp(table->output_file, "READ LOCK ACQUIRED\n");
    rwlock_acquire_readlock(&table->rwlock);
    table->lock_acquisitions++;

    // Print all records
    hashRecord *current = table->head;
    while (current) {
        fprintf(table->output_file, "%u,%s,%u\n", current->hash, current->name, current->salary);
        current = current->next;
    }

    // Release read lock
    rwlock_release_readlock(&table->rwlock);
    table->lock_releases++;
    log_with_timestamp(table->output_file, "READ LOCK RELEASED\n");
}

// Print a single record
void print_record(FILE *file, hashRecord *record) {
    fprintf(file, "Name: %s, Salary: %u\n", record->name, record->salary);
}

void print_summary(HashTable *table) {
    fprintf(table->output_file, "\nNumber of lock acquisitions: %d\n", table->lock_acquisitions);
    fprintf(table->output_file, "Number of lock releases: %d\n", table->lock_releases);

    // Print the final hash table content
    log_with_timestamp(table->output_file, "READ LOCK ACQUIRED\n");
    rwlock_acquire_readlock(&table->rwlock);
    table->lock_acquisitions++;

    hashRecord *current = table->head;
    while (current) {
        fprintf(table->output_file, "%u,%s,%u\n", current->hash, current->name, current->salary);
        current = current->next;
    }

    rwlock_release_readlock(&table->rwlock);
    table->lock_releases++;
    log_with_timestamp(table->output_file, "READ LOCK RELEASED\n");
}

// Free all resources used by the hash table
void free_table(HashTable *table) {

    // Free all records
    hashRecord *current = table->head;
    while (current) {
        hashRecord *temp = current;
        current = current->next;
        free(temp);
    }
    // Close the output file
    fclose(table->output_file);

    // Destroy synchronization objects
    rwlock_destroy(&table->rwlock);
    pthread_mutex_destroy(&table->cv_mutex);
    pthread_cond_destroy(&table->insert_complete);

    // Free the table structure itself
    free(table);
    
}

