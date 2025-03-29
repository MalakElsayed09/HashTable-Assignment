#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "hashTable.h"

#define HASH_SIZE 10

// Jenkins' one-at-a-time hash function
unsigned int jenkins_hash(const char *key) {
    unsigned int hash = 0;
    while (*key) {
        hash += *key++;
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash % HASH_SIZE;
}

// Initialize the hash table
void hashtable_init(HashTable *ht) {
    for (int i = 0; i < HASH_SIZE; i++) {
        ht->buckets[i] = NULL;
        pthread_rwlock_init(&ht->locks[i], NULL);
    }
}

// Insert a record into the hash table
void hashtable_insert(HashTable *ht, const char *key, int value) {
    unsigned int index = jenkins_hash(key);
    pthread_rwlock_wrlock(&ht->locks[index]);
    printf("Lock acquired for insert: %s\n", key);
    
    HashRecord *new_record = malloc(sizeof(HashRecord));
    strcpy(new_record->key, key);
    new_record->value = value;
    new_record->next = ht->buckets[index];
    ht->buckets[index] = new_record;
    
    printf("Inserted: %s -> %d\n", key, value);
    
    pthread_rwlock_unlock(&ht->locks[index]);
    printf("Lock released for insert: %s\n", key);
}

// Search for a record in the hash table
int hashtable_search(HashTable *ht, const char *key) {
    unsigned int index = jenkins_hash(key);
    pthread_rwlock_rdlock(&ht->locks[index]);
    printf("Lock acquired for search: %s\n", key);
    
    HashRecord *curr = ht->buckets[index];
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            printf("Found: %s -> %d\n", key, curr->value);
            pthread_rwlock_unlock(&ht->locks[index]);
            printf("Lock released for search: %s\n", key);
            return curr->value;
        }
        curr = curr->next;
    }
    
    printf("Not found: %s\n", key);
    pthread_rwlock_unlock(&ht->locks[index]);
    printf("Lock released for search: %s\n", key);
    return -1;
}

// Delete a record from the hash table
void hashtable_delete(HashTable *ht, const char *key) {
    unsigned int index = jenkins_hash(key);
    pthread_rwlock_wrlock(&ht->locks[index]);
    printf("Lock acquired for delete: %s\n", key);
    
    HashRecord *curr = ht->buckets[index];
    HashRecord *prev = NULL;
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                ht->buckets[index] = curr->next;
            }
            free(curr);
            printf("Deleted: %s\n", key);
            pthread_rwlock_unlock(&ht->locks[index]);
            printf("Lock released for delete: %s\n", key);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    
    printf("Not found for delete: %s\n", key);
    pthread_rwlock_unlock(&ht->locks[index]);
    printf("Lock released for delete: %s\n", key);
}

// Destroy the hash table
void free_table(HashTable *table) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        pthread_rwlock_wrlock(&table->locks[i]); // Lock before modifying

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
