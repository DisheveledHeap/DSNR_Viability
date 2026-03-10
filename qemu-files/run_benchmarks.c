#include <numa.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// bind threads to nodes
#define NUM_NODES 4

typedef struct {
    long value
} replica_t;

replica_t *replicas[NUM_NODES];

void pin_thread_to_node(int node) {
    struct bitmask *mask = numa_allocate_cpumask();
    numa_node_to_cpus(node, mask);
    numa_sched_setaffinity(0, mask);
    numa_free_cpumask(mask);
}

int get_local_node() {
    return numa_node_of_cpu(sched_getcpu());
}

long read_counter() {
    int node = get_local_node();
    return replicas[node]->value;
}

void increment_counter() {
    for (int i = 0; i < NUM_NODES; i++) {
        __sync_fetch_and_add(&replicas[i]->value, 1);
    }
}

void* worker(void* arg) {
    int node = *(int*)arg;
    pin_thread_to_node(node);

    for (int i = 0; i < 1000000; i++) {
        increment_counter();
    }

    return NULL;
}

int main() {
    if (numa_available() < 0) {
        fprintf(stderr, "NUMA not available\n");
        return 1;
    }

    // --- 1️⃣ Allocate replicas BEFORE spawning threads ---
    for (int i = 0; i < NUM_NODES; i++) {
        replicas[i] = numa_alloc_onnode(sizeof(replica_t), i);
        replicas[i]->value = 0;
    }

    // --- 2️⃣ Spawn one thread per node ---
    pthread_t threads[NUM_NODES];
    int node_ids[NUM_NODES] = {0,1,2,3};

    for (int i = 0; i < NUM_NODES; i++) {
        pthread_create(&threads[i], NULL, worker, &node_ids[i]);
    }

    for (int i = 0; i < NUM_NODES; i++) {
        pthread_join(threads[i], NULL);
    }

    // --- 3️⃣ Print final values ---
    for (int i = 0; i < NUM_NODES; i++) {
        printf("Replica %d: %ld\n", i, replicas[i]->value);
    }

    return 0;
}