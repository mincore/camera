/* ===================================================
 * Copyright (C) 2015 chenshuangping All Right Reserved.
 *      Author: mincore@163.com
 *    Filename: queue.h
 *     Created: 2015-07-13 07:31
 * Description:
 * ===================================================
 */
#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "list.h"

#define atomic_set(p, v)    (*(p) = (v))
#define atomic_read(p)      (*(p))
#define atomic_add(p, n)    (__sync_add_and_fetch((p), (n)))
#define atomic_sub(p, n)    (__sync_sub_and_fetch((p), (n)))

typedef struct Queue {
    size_t max_count;
    size_t count;
    struct list_head queue;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} Queue;

#define QUEUE_INIT(name, len) {                   \
    .max_count = (len) ? (len) : 0xFFFFFFFF,      \
    .count = 0,                                   \
    .queue = LIST_HEAD_INIT((name).queue),        \
    .lock = PTHREAD_MUTEX_INITIALIZER,            \
    .cond = PTHREAD_COND_INITIALIZER,             \
}

#define QUEUE(name, len) \
    struct Queue name = QUEUE_INIT(name, len)

static inline void queue_init(Queue *q, size_t max_count)
{
    INIT_LIST_HEAD(&q->queue);
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->count = 0;
    q->max_count = max_count == 0 ? 0xFFFFFFFF : max_count;
}

static inline void queue_deinit(Queue *q)
{
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
}

static inline int queue_push(Queue *q, struct list_head *entry)
{
    int ret = -1;

    pthread_mutex_lock(&q->lock);
    if (q->count < q->max_count) {
        list_add_tail(entry, &q->queue);
        atomic_add(&q->count, 1);
        ret = 0;
        pthread_cond_signal(&q->cond);
    }
    pthread_mutex_unlock(&q->lock);

    return ret;
}

static inline int queue_push_head(Queue *q, struct list_head *entry)
{
    int ret = -1;

    pthread_mutex_lock(&q->lock);
    if (q->count < q->max_count) {
        list_add(entry, &q->queue);
        atomic_add(&q->count, 1);
        ret = 0;
        pthread_cond_signal(&q->cond);
    }
    pthread_mutex_unlock(&q->lock);

    return ret;
}

static inline struct list_head* queue_pop(Queue *q)
{
    struct list_head *head;

    pthread_mutex_lock(&q->lock);
    while (list_empty(&q->queue)) {
        pthread_cond_wait(&q->cond, &q->lock);
    }
    head = q->queue.next;
    list_del(head);
    atomic_sub(&q->count, 1);
    pthread_mutex_unlock(&q->lock);

    return head;
}

static inline struct list_head* queue_pop_nowait(Queue *q)
{
    struct list_head *head = NULL;

    pthread_mutex_lock(&q->lock);
    if (!list_empty(&q->queue)) {
        head = q->queue.next;
        list_del(head);
        atomic_sub(&q->count, 1);
    }
    pthread_mutex_unlock(&q->lock);

    return head;
}

/* (peek + del): in one consumer case */
static inline struct list_head* queue_peek_nowait(Queue *q)
{
    struct list_head *head = NULL;

    pthread_mutex_lock(&q->lock);
    if (!list_empty(&q->queue)) {
        head = q->queue.next;
    }
    pthread_mutex_unlock(&q->lock);

    return head;
}

static inline void queue_wait(Queue *q)
{
    pthread_mutex_lock(&q->lock);
    pthread_cond_wait(&q->cond, &q->lock);
    pthread_mutex_unlock(&q->lock);
}

static inline void queue_wait_empty(Queue *q)
{
    pthread_mutex_lock(&q->lock);
    while (!list_empty(&q->queue))
        pthread_cond_wait(&q->cond, &q->lock);
    pthread_mutex_unlock(&q->lock);
}

static inline void queue_signal(Queue *q)
{
    pthread_mutex_lock(&q->lock);
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

static inline void queue_del(Queue *q, struct list_head *entry)
{
    pthread_mutex_lock(&q->lock);
    list_del(entry);
    atomic_sub(&q->count, 1);
    pthread_mutex_unlock(&q->lock);
}

static inline int queue_count(Queue *q)
{
    return atomic_read(&q->count);
}

#endif
