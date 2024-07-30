#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>
#include <stdatomic.h>

#include "queue.h"

ItemsQueue items_queue;
ThreadQueue threads_queue;

void initQueue(void) {
    /* Initialization of the items' queue */
    items_queue.head = NULL;
    items_queue.tail = NULL;
    atomic_init(&items_queue.size, 0);
    atomic_init(&items_queue.visited, 0);
    mtx_init(&items_queue.lock, mtx_plain);
    cnd_init(&items_queue.not_empty);

    /* Initialization of the threads' queue */
    threads_queue.head = NULL;
    threads_queue.tail = NULL;
    atomic_init(&threads_queue.size, 0);
    mtx_init(&threads_queue.lock, mtx_plain);
}

void enqueue(void* item) {
    ItemNode* new_item = (ItemNode*)malloc(sizeof(ItemNode));
    new_item->data = item;
    new_item->next = NULL;

    mtx_lock(&threads_queue.lock);
    if (threads_queue.head != NULL) {
        ThreadNode* thrd_to_wake = threads_queue.head; // wake the oldest thread in the queue
        threads_queue.head = threads_queue.head->next;
        if (threads_queue.head == NULL) {
            threads_queue.tail = NULL;
        }
        mtx_unlock(&threads_queue.lock);
        cnd_signal(&items_queue.not_empty);
        free(thrd_to_wake);
        return;
    }
    mtx_unlock(&threads_queue.lock);

    mtx_lock(&items_queue.lock);
    if (atomic_load(&items_queue.size) == 0) {
        items_queue.head = new_item;
        items_queue.tail = new_item;
    } else {
        items_queue.tail->next = new_item;
        items_queue.tail = new_item;
    }
    atomic_fetch_add(&items_queue.size, 1);
    atomic_fetch_add(&items_queue.visited, 1);
    mtx_unlock(&items_queue.lock);
    cnd_broadcast(&items_queue.not_empty);
}

void* dequeue(void) {
    mtx_lock(&items_queue.lock);
    while (atomic_load(&items_queue.size) == 0) {
        ThreadNode* newThreadNode = (ThreadNode*)malloc(sizeof(ThreadNode));
        newThreadNode->thread = thrd_current();
        newThreadNode->next = NULL;

        mtx_lock(&threads_queue.lock);
        if (threads_queue.tail != NULL) {
            threads_queue.tail->next = newThreadNode;
        }
        threads_queue.tail = newThreadNode;
        if (threads_queue.head == NULL) {
            threads_queue.head = newThreadNode;
        }
        atomic_fetch_add(&threads_queue.size, 1);
        mtx_unlock(&threads_queue.lock);

        cnd_wait(&items_queue.not_empty, &items_queue.lock);
    }

    ItemNode* temp = items_queue.head;
    void* item = temp->data;
    items_queue.head = items_queue.head->next;
    if (items_queue.head == NULL) {
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
    if (atomic_load(&items_queue.size) == 0) {
        mtx_unlock(&items_queue.lock);
        return false;
    }

    ItemNode* temp = items_queue.head;
    *item = temp->data;
    items_queue.head = items_queue.head->next;
    if (items_queue.head == NULL) {
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
    while (items_queue.head) {
        ItemNode* tmp = items_queue.head;
        items_queue.head = items_queue.head->next;
        free(tmp);
    }
    mtx_destroy(&items_queue.lock);
    cnd_destroy(&items_queue.not_empty);

    while (threads_queue.head) {
        ThreadNode* tmp = threads_queue.head;
        threads_queue.head = threads_queue.head->next;
        free(tmp);
    }
    mtx_destroy(&threads_queue.lock);
}
