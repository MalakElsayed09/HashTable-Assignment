#include "hashTable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_table(HashTable *table) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        table->buckets[i] = NULL;
        pthread_rwlock_init(&table->locks[i], NULL);
    }
}

uint32_t jenkins_one_at_a_time_hash(const char *key) {
    uint32_t hash = 0;
    while (*key) {
        hash += (unsigned char)(*key);
        hash += (hash << 10);
        hash ^= (hash >> 6);
        key++;
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % HASH_TABLE_SIZE;
}

void insert(HashTable *table, const char *key, uint32_t salary) {
    uint32_t index = jenkins_one_at_a_time_hash(key);
    pthread_rwlock_wrlock(&table->locks[index]);
    printf("Write lock acquired for insert on index %u\n", index);
    
    hashRecord *newRecord = (hashRecord *)malloc(sizeof(hashRecord));
    if (!newRecord) {
        perror("Failed to allocate memory");
        pthread_rwlock_unlock(&table->locks[index]);
        return;
    }
    newRecord->hash = index;
    strncpy(newRecord->name, key, 50);
    newRecord->salary = salary;
    newRecord->next = table->buckets[index];
    table->buckets[index] = newRecord;
    
    printf("Inserted record: %s with salary %u at index %u\n", key, salary, index);
    pthread_rwlock_unlock(&table->locks[index]);
    printf("Write lock released for insert on index %u\n", index);
}

void delete(HashTable *table, const char *key) {
    uint32_t index = jenkins_one_at_a_time_hash(key);
    pthread_rwlock_wrlock(&table->locks[index]);
    printf("Write lock acquired for delete on index %u\n", index);
    
    hashRecord *prev = NULL;
    hashRecord *current = table->buckets[index];
    while (current) {
        if (strcmp(current->name, key) == 0) {
            if (prev) {
                prev->next = current->next;
            } else {
                table->buckets[index] = current->next;
            }
            free(current);
            printf("Deleted record: %s from index %u\n", key, index);
            pthread_rwlock_unlock(&table->locks[index]);
            printf("Write lock released for delete on index %u\n", index);
            return;
        }
        prev = current;
        current = current->next;
    }
    
    printf("Record not found for deletion: %s\n", key);
    pthread_rwlock_unlock(&table->locks[index]);
    printf("Write lock released for delete on index %u\n", index);
}

hashRecord *search(HashTable *table, const char *key) {
    uint32_t index = jenkins_one_at_a_time_hash(key);
    pthread_rwlock_rdlock(&table->locks[index]);
    printf("Read lock acquired for search on index %u\n", index);
    
    hashRecord *current = table->buckets[index];
    while (current) {
        if (strcmp(current->name, key) == 0) {
            printf("Found record: %s with salary %u at index %u\n", key, current->salary, index);
            pthread_rwlock_unlock(&table->locks[index]);
            printf("Read lock released for search on index %u\n", index);
            return current;
        }
        current = current->next;
    }
    
    printf("Record not found: %s\n", key);
    pthread_rwlock_unlock(&table->locks[index]);
    printf("Read lock released for search on index %u\n", index);
    return NULL;
}

void free_table(HashTable *table) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        pthread_rwlock_wrlock(&table->locks[i]);
        hashRecord *current = table->buckets[i];
        while (current) {
            hashRecord *next = current->next;
            free(current);
            current = next;
        }
        table->buckets[i] = NULL;
        pthread_rwlock_unlock(&table->locks[i]);
        pthread_rwlock_destroy(&table->locks[i]);
    }
}

