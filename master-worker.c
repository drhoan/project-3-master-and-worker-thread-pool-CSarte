#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>

int item_to_produce, curr_buf_size;
int total_items, max_buf_size, num_workers, num_masters;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

int *buffer;
int producers_done = 0; // Flag to mark when producers are done

void print_produced(int num, int master) {
    printf("Produced %d by master %d\n", num, master);
}

void print_consumed(int num, int worker) {
    printf("Consumed %d by worker %d\n", num, worker);
}

// Produce items and place in buffer
void *generate_requests_loop(void *data) {
    int thread_id = *((int *)data);

    while (1) {
        pthread_mutex_lock(&mutex);
        if (item_to_produce >= total_items) {
            // Once we've produced all items, stop.
            pthread_mutex_unlock(&mutex);
            break;
        }

        while (curr_buf_size == max_buf_size) {
            // Wait for space in the buffer
            pthread_cond_wait(&not_full, &mutex);
        }

        // Produce a new item and increment the counter
        int item = item_to_produce;
        item_to_produce++; // Increment after we've "taken" the item
        buffer[curr_buf_size++] = item;
        print_produced(item, thread_id);

        // Signal the consumers that an item is available
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

// Consume items from the buffer
void *consume_requests_loop(void *data) {
    int thread_id = *((int *)data);

    while (1) {
        pthread_mutex_lock(&mutex);

        while (curr_buf_size == 0) {
            // Wait if the buffer is empty and no more items will be produced
            if (item_to_produce >= total_items && producers_done) {
                // If all items are produced and the buffer is empty, exit
                pthread_mutex_unlock(&mutex);
                return NULL;
            }
            pthread_cond_wait(&not_empty, &mutex);
        }

        // Consume an item from the buffer
        int item = buffer[--curr_buf_size];
        print_consumed(item, thread_id);

        // Signal the producer that space is available
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int *master_thread_id;
    pthread_t *master_thread;
    item_to_produce = 0;
    curr_buf_size = 0;

    int i;

    if (argc < 5) {
        printf("./master-worker #total_items #max_buf_size #num_workers #masters e.g. ./exe 10000 1000 4 3\n");
        exit(1);
    } else {
        num_masters = atoi(argv[4]);
        num_workers = atoi(argv[3]);
        total_items = atoi(argv[1]);
        max_buf_size = atoi(argv[2]);
    }

    buffer = (int *)malloc(sizeof(int) * max_buf_size);

    // Create master producer threads
    master_thread_id = (int *)malloc(sizeof(int) * num_masters);
    master_thread = (pthread_t *)malloc(sizeof(pthread_t) * num_masters);
    for (i = 0; i < num_masters; i++) {
        master_thread_id[i] = i;
        pthread_create(&master_thread[i], NULL, generate_requests_loop, (void *)&master_thread_id[i]);
    }

    // Create worker consumer threads
    int *worker_thread_id = malloc(sizeof(int) * num_workers);
    pthread_t *worker_thread = malloc(sizeof(pthread_t) * num_workers);
    for (i = 0; i < num_workers; i++) {
        worker_thread_id[i] = i;
        pthread_create(&worker_thread[i], NULL, consume_requests_loop, (void *)&worker_thread_id[i]);
    }

    // Wait for all threads to finish (producers)
    for (i = 0; i < num_masters; i++) {
        pthread_join(master_thread[i], NULL);
        printf("master %d joined\n", i);
    }

    // Set producers_done flag
    pthread_mutex_lock(&mutex);
    producers_done = 1;
    pthread_cond_broadcast(&not_empty); // Wake up any waiting consumers
    pthread_mutex_unlock(&mutex);

    // Wait for all threads to finish (consumers)
    for (i = 0; i < num_workers; i++) {
        pthread_join(worker_thread[i], NULL);
        printf("worker %d joined\n", i);
    }

    // Free memory
    free(worker_thread);
    free(worker_thread_id);

    free(buffer);
    free(master_thread_id);
    free(master_thread);

    return 0;
}
