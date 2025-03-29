#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "hashTable.h"

#define TABLE_SIZE 100  // Consistent with hashtable.h

// Jenkins' one-at-a-time hash function
uint32_t jenkins_one_at_a_time_hash(const char *key) {
    uint32_t hash = 0;
    while (*key) {
        hash += *key++;
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash % TABLE_SIZE;
}

// Initialize the hash table
void init_table(HashTable *table) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        table->buckets[i] = NULL;
        pthread_rwlock_init(&table->locks[i], NULL);
    }
}

// Insert a record into the hash table
void insert(HashTable *table, const char *name, uint32_t salary) {
    uint32_t index = jenkins_one_at_a_time_hash(name);
    pthread_rwlock_wrlock(&table->locks[index]);
    printf("Lock acquired for insert: %s\n", name);
    
    hashRecord *new_record = malloc(sizeof(hashRecord));
    strcpy(new_record->name, name);
    new_record->salary = salary;
    new_record->next = table->buckets[index];
    table->buckets[index] = new_record;
    
    printf("Inserted: %s -> %u\n", name, salary);
    
    pthread_rwlock_unlock(&table->locks[index]);
    printf("Lock released for insert: %s\n", name);
}

// Search for a record in the hash table
hashRecord *search(HashTable *table, const char *name) {
    uint32_t index = jenkins_one_at_a_time_hash(name);
    pthread_rwlock_rdlock(&table->locks[index]);
    printf("Lock acquired for search: %s\n", name);
    
    hashRecord *curr = table->buckets[index];
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            printf("Found: %s -> %u\n", name, curr->salary);
            pthread_rwlock_unlock(&table->locks[index]);
            printf("Lock released for search: %s\n", name);
            return curr;
        }
        curr = curr->next;
    }
    
    printf("Not found: %s\n", name);
    pthread_rwlock_unlock(&table->locks[index]);
    printf("Lock released for search: %s\n", name);
    return NULL;
}

// Delete a record from the hash table
void delete(HashTable *table, const char *name) {
    uint32_t index = jenkins_one_at_a_time_hash(name);
    pthread_rwlock_wrlock(&table->locks[index]);
    printf("Lock acquired for delete: %s\n", name);
    
    hashRecord *curr = table->buckets[index];
    hashRecord *prev = NULL;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                table->buckets[index] = curr->next;
            }
            free(curr);
            printf("Deleted: %s\n", name);
            pthread_rwlock_unlock(&table->locks[index]);
            printf("Lock released for delete: %s\n", name);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    
    printf("Not found for delete: %s\n", name);
    pthread_rwlock_unlock(&table->locks[index]);
    printf("Lock released for delete: %s\n", name);
}

// Destroy the hash table
void free_table(HashTable *table) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        pthread_rwlock_wrlock(&table->locks[i]);

        hashRecord *current = table->buckets[i];
        while (current) {
            hashRecord *temp = current;
            current = current->next;
            free(temp);
        }
        table->buckets[i] = NULL;

        pthread_rwlock_unlock(&table->locks[i]); 
        pthread_rwlock_destroy(&table->locks[i]); // Destroy lock
    }
}
