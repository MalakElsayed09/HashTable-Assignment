#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#define TABLE_SIZE 100

typedef struct hash_struct {
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

typedef struct {
    hashRecord *buckets[TABLE_SIZE];
    pthread_rwlock_t locks[TABLE_SIZE];
} HashTable;

void init_table(HashTable *table);
uint32_t jenkins_one_at_a_time_hash(const char *key);
void insert(HashTable *table, const char *name, uint32_t salary);
void delete(HashTable *table, const char *name);
hashRecord *search(HashTable *table, const char *name);
void free_table(HashTable *table);

#endif // HASHTABLE_H

