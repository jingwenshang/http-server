#include "server.h"



void queue_init(task_queue_t *q) {
    q->head  = 0;
    q->tail  = 0;
    q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_push(task_queue_t *q, int fd) {
    pthread_mutex_lock(&q->lock);

    /* Block if queue is full (backpressure) */
    while (q->count == QUEUE_CAPACITY) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    q->fds[q->tail] = fd;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

int queue_pop(task_queue_t *q) {
    pthread_mutex_lock(&q->lock);

    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    int fd = q->fds[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);

    return fd;
}


void *worker_main(void *arg) {
    server_t *srv = (server_t *)arg;

    while (srv->running) {
        int client_fd = queue_pop(&srv->queue);

        if (client_fd < 0) {
            continue; 
        }

       
        __sync_fetch_and_add(&srv->active_connections, 1);

        handle_connection(srv, client_fd);

        __sync_fetch_and_sub(&srv->active_connections, 1);

        close(client_fd);
    }

    return NULL;
}
