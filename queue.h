#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

/*functions*/
void initQueue(void);
void destroyQueue(void);
void enqueue(void*);
void* dequeue(void);
bool tryDequeue(void**);
size_t size(void);
size_t waiting(void);
size_t visited(void);

/*an item node*/
typedef struct ItemNode {
    void *data;
    struct ItemNode *next;
} ItemNode;

/*a thread node*/
typedef struct ThreadNode {
    thrd_t thread;
    struct ThreadNode *next;
} ThreadNode;

/*items queue structure*/
typedef struct ItemsQueue {
    size_t size;
    ItemNode *head; /*oldest item in the queue*/
    ItemNode *tail; /*newest item in the queue*/
    atomic_size_t waiting; /*num of threads waiting*/
    atomic_size_t visited; /*num of items visited*/
    mtx_t lock; /*the lock that distributes the time window for each thread*/
    cnd_t not_empty; 
} ItemsQueue;

/*threads queue structure*/
typedef struct ThreadQueue {
    ThreadNode *head; //oldest item in the queue
    ThreadNode *tail; //newest item in the queue
    atomic_size_t size;
    mtx_t lock;
} ThreadQueue;

#endif // QUEUE_H
