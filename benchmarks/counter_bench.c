/*
 * counter_bench.c
 *
 * What this does:
 *   Multiple threads all increment ONE shared counter.
 *   Only one thread can increment at a time (mutex lock).
 *   More threads = more waiting = slower = contention problem.
 *
 * What we measure:
 *   Wall clock time and ops/sec for 1,2,4,8,16 threads.
 *
 *
 * Compile: gcc -O2 -o counter_bench counter_bench.c -lpthread -lnuma
 * Run:     ./counter_bench
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <numa.h>

/* ── How many times each thread increments the counter ─── */
#define ITERATIONS  2000000
#define MAX_THREADS 16

/* ── The shared counter ───────────────────────────────────
 * This is the single point of contention.
 * Every thread fights to lock and increment this.
 * Padded to 64 bytes = one full CPU cache line.
 * This prevents the counter sharing a cache line with
 * other variables (false sharing), keeping results clean.
 * ─────────────────────────────────────────────────────── */
typedef struct {
    volatile uint64_t value;     /* the actual counter        */
    pthread_mutex_t   lock;      /* only one thread at a time */
    char              pad[40];   /* fills to 64 bytes         */
} Counter;

/* One global counter — ALL threads share this */
Counter shared;

/* ── What each thread needs to know ────────────────────── */
typedef struct {
    int thread_id;
} ThreadArg;

/* ── Worker: each thread runs this function ─────────────── */
void *worker(void *arg)
{
    ThreadArg *t = (ThreadArg *)arg;
    (void)t; /* unused for now — will use in NR version */

    for (int i = 0; i < ITERATIONS; i++) {
        pthread_mutex_lock(&shared.lock);
        shared.value++;                    /* critical section */
        pthread_mutex_unlock(&shared.lock);
    }
    return NULL;
}

/* ── Run benchmark with a given number of threads ────────── */
void run(int num_threads, FILE *csv)
{
    pthread_t  threads[MAX_THREADS];
    ThreadArg  args[MAX_THREADS];

    /* Reset counter before each run */
    shared.value = 0;
    pthread_mutex_init(&shared.lock, NULL);

    /* Start timer */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Spawn all threads */
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    /* Stop timer */
    clock_gettime(CLOCK_MONOTONIC, &end);

    /* ── Calculate results ── */
    double wall_sec = (end.tv_sec  - start.tv_sec) +
                      (end.tv_nsec - start.tv_nsec) / 1e9;

    uint64_t expected = (uint64_t)num_threads * ITERATIONS;
    uint64_t actual   = shared.value;
    double   ops_sec  = expected / wall_sec;

    /* Correctness: did every increment get counted? */
    int correct = (actual == expected);

    /* Print result */
    printf("  threads=%2d  time=%7.3fs  ops/sec=%12.0f  "
           "expected=%-12lu  got=%-12lu  correct=%s\n",
           num_threads,
           wall_sec,
           ops_sec,
           expected,
           actual,
           correct ? "YES" : "NO <-- BUG");

    /* Save to CSV */
    fprintf(csv, "%d,%.4f,%.0f,%s\n",
            num_threads, wall_sec, ops_sec,
            correct ? "YES" : "NO");

    pthread_mutex_destroy(&shared.lock);
}

/* ── Main ───────────────────────────────────────────────── */
int main()
{
    /* Check NUMA */
    int numa_nodes = 1;
    if (numa_available() != -1)
        numa_nodes = numa_max_node() + 1;

    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║           SHARED COUNTER BENCHMARK                  ║\n");
    printf("║  NUMA nodes: %-3d                                    ║\n",
           numa_nodes);
    printf("║  Iterations per thread: %-7d                      ║\n",
           ITERATIONS);
    printf("║  Goal: ops/sec should DROP as threads increase      ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Open CSV file for results */
    // mkdir("results", 0755);
    FILE *csv = fopen("results/counter_results.csv", "w");
    if (!csv) { perror("Cannot open results file"); return 1; }
    fprintf(csv, "threads,wall_sec,ops_per_sec,correct\n");

    printf("  %-10s %-12s %-14s %-14s %-14s %s\n",
           "Threads", "Time(s)", "Ops/sec",
           "Expected", "Got", "Correct");
    printf("  %s\n", "─────────────────────────────────────────"
                      "──────────────────────────────");

    /* Run with increasing thread counts */
    int counts[] = {1, 2, 4, 8, 16};
    for (int i = 0; i < 5; i++)
        run(counts[i], csv);

    fclose(csv);

    printf("\n  Results saved to results/counter_results.csv\n");
    printf("\n  What to look for:\n");
    printf("  → ops/sec dropping = contention problem confirmed\n");
    printf("  → correct=YES      = no lost increments (mutex works)\n\n");

    return 0;
}
