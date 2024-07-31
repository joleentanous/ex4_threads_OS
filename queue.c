#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdio.h>

#include "queue.h"

/* an item node */
typedef struct ItemNode {
    void *data;
    struct ItemNode *next;
} ItemNode;

/* a thread node */
typedef struct ThreadNode {
    thrd_t thread;
    struct ThreadNode *next;
    cnd_t cnd; /* each thread has its own condition variable */
} ThreadNode;

/* items queue structure */
typedef struct ItemsQueue {
    ItemNode *head; /* oldest item in the queue */
    ItemNode *tail; /* newest item in the queue */
    atomic_size_t size;
    atomic_size_t waiting; /* number of threads waiting */
    atomic_size_t visited; /* number of items visited */
    mtx_t lock; /* lock for synchronizing access */
} ItemsQueue;

/* threads queue structure */
typedef struct ThreadQueue {
    ThreadNode *head; /* oldest thread in the queue */
    ThreadNode *tail; /* newest thread in the queue */
    atomic_size_t size;
    mtx_t lock; /* lock for synchronizing access */
} ThreadQueue;

ItemsQueue items_queue;
ThreadQueue threads_queue;

void initQueue(void) {
    /* Initialize items queue */
    items_queue.head = NULL;
    items_queue.tail = NULL;
    atomic_init(&items_queue.size, 0);
    atomic_init(&items_queue.visited, 0);
    mtx_init(&items_queue.lock, mtx_plain);

    /* Initialize threads queue */
    threads_queue.head = NULL;
    threads_queue.tail = NULL;
    atomic_init(&threads_queue.size, 0);
    mtx_init(&threads_queue.lock, mtx_plain);
}

void enqueue(void* item) {
    /*creates a new item node*/
    ItemNode* new_item = (ItemNode*)malloc(sizeof(ItemNode));
    if (!new_item) {
        fprintf(stderr, "Failed to allocate memory for new item\n");
        abort();
    }
    new_item->data = item;
    new_item->next = NULL;

    mtx_lock(&items_queue.lock);
    /*adds item to items_queue*/
    if (items_queue.tail) {
        items_queue.tail->next = new_item;
    } else {
        items_queue.head = new_item;
    }
    items_queue.tail = new_item;
    atomic_fetch_add(&items_queue.size, 1);

    /* Signal a waiting thread if there is one */
    if (atomic_load(&threads_queue.size) > 0) {
        mtx_lock(&threads_queue.lock);
        if (threads_queue.head) {
            cnd_signal(&threads_queue.head->cnd);
        }
        mtx_unlock(&threads_queue.lock);
    }

    mtx_unlock(&items_queue.lock);
}

void* dequeue(void) {
    mtx_lock(&items_queue.lock);
    while (atomic_load(&items_queue.size) == 0) {
        /* Create a new thread node */
        ThreadNode* new_thread = (ThreadNode*)malloc(sizeof(ThreadNode));
        if (!new_thread) {
            fprintf(stderr, "Failed to allocate memory for new thread node\n");
            abort();
        }
        cnd_init(&new_thread->cnd);
        new_thread->next = NULL;

        mtx_lock(&threads_queue.lock);
        /* Insert thread to the thread's queue */
        if (threads_queue.tail) {
            threads_queue.tail->next = new_thread;
        } else {
            threads_queue.head = new_thread;
        }
        threads_queue.tail = new_thread;
        atomic_fetch_add(&threads_queue.size, 1);
        mtx_unlock(&threads_queue.lock);

        cnd_wait(&new_thread->cnd, &items_queue.lock);

        mtx_lock(&threads_queue.lock);
        /* Remove the thread node from the queue */
        if (threads_queue.head == new_thread) {
            threads_queue.head = new_thread->next;
            if (!threads_queue.head) {
                threads_queue.tail = NULL;
            }
        } else {
            ThreadNode* prev = threads_queue.head;
            while (prev && prev->next != new_thread) {
                prev = prev->next;
            }
            if (prev) {
                prev->next = new_thread->next;
                if (!prev->next) {
                    threads_queue.tail = prev;
                }
            }
        }
        atomic_fetch_sub(&threads_queue.size, 1);
        cnd_destroy(&new_thread->cnd);
        free(new_thread);
        mtx_unlock(&threads_queue.lock);
    }

    ItemNode* temp = items_queue.head;
    void* item = temp->data;
    items_queue.head = items_queue.head->next;
    if (!items_queue.head) {
        items_queue.tail = NULL;
    }
    atomic_fetch_sub(&items_queue.size, 1);
    atomic_fetch_add(&items_queue.visited, 1);
    free(temp);

    mtx_unlock(&items_queue.lock);
    return item;
}

bool tryDequeue(void** item) {
    mtx_lock(&items_queue.lock);
    /*empty queue*/
    if (atomic_load(&items_queue.size) == 0) {
        mtx_unlock(&items_queue.lock);
        return false;
    }

    /*delete the oldest item*/
    ItemNode* temp = items_queue.head;
    *item = temp->data;
    items_queue.head = items_queue.head->next;
    if (!items_queue.head) {
        items_queue.tail = NULL;
    }
    atomic_fetch_sub(&items_queue.size, 1);
    atomic_fetch_add(&items_queue.visited, 1);
    free(temp);

    mtx_unlock(&items_queue.lock);
    return true;
}

size_t size(void) {
    return atomic_load(&items_queue.size);
}

size_t waiting(void) {
    return atomic_load(&threads_queue.size);
}

size_t visited(void) {
    return atomic_load(&items_queue.visited);
}

void destroyQueue(void) {
    mtx_lock(&items_queue.lock);
    while (items_queue.head) {
        ItemNode* tmp = items_queue.head;
        items_queue.head = items_queue.head->next;
        free(tmp);
    }

    mtx_lock(&threads_queue.lock);
    while (threads_queue.head) {
        ThreadNode* tmp = threads_queue.head;
        threads_queue.head = threads_queue.head->next;
        cnd_destroy(&tmp->cnd);
        free(tmp);
    }
    mtx_unlock(&threads_queue.lock);
    mtx_destroy(&threads_queue.lock);

    mtx_unlock(&items_queue.lock);
    mtx_destroy(&items_queue.lock);
}
