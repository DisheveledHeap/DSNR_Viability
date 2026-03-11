/*
 * pascal_bench.c
 *
 * What this does:
 *   Computes Pascal's Triangle using multiple threads.
 *   Each row is divided among all threads — they work in parallel.
 *   Threads CANNOT start row N until ALL threads finish row N-1.
 *   This is enforced by a barrier (all threads must meet here).
 *
 *   The previous row is shared data that every thread reads.
 *   This is the contention point — in a disaggregated system,
 *   that previous row would sit in remote memory, and every
 *   thread would pay the network latency to read it.
 *   Node Replication (Stage 3) gives each node a local copy.
 *
 * What we measure:
 *   Wall clock time for 1,2,4,8,16 threads.
 *   Also verify: every cell must match single-threaded reference.
 *
 *
 * Compile: gcc -O2 -o pascal_bench pascal_bench.c -lpthread -lnuma
 * Run:     ./pascal_bench
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <numa.h>

/* ── Triangle size ────────────────────────────────────────
 * 2000 rows gives enough work per row to make parallelism
 * meaningful. Each row N has N+1 cells, so row 2000 has
 * 2001 cells for threads to split.
 * Use uint64_t so values wrap around cleanly (they get huge).
 * ─────────────────────────────────────────────────────── */
#define NUM_ROWS       2000
#define MAX_COLS       2000
#define MAX_THREADS    16
#define MAX_NUMA_NODES 16

typedef uint64_t cell_t;

/* ── Triangle storage ─────────────────────────────────────
 * All threads read and write to this shared array.
 * row_barrier ensures no thread starts row N before
 * all threads have finished row N-1.
 * ─────────────────────────────────────────────────────── */
cell_t           *triangle_shared;
cell_t          **triangle_nodes;
pthread_barrier_t row_barrier;

int use_replication = 0;
int numa_nodes = 1;

static inline cell_t *tri(cell_t* base, int r, int c)
    return &base[r * MAX_COLS + c];

/* ── Single-threaded reference ────────────────────────────
 * Computed once at startup.
 * Used to verify parallel result is correct.
 * ─────────────────────────────────────────────────────── */
cell_t reference[NUM_ROWS][MAX_COLS];

void compute_reference()
{
    /*
     * Pascal's rule:
     *   triangle[r][0]   = 1              (left edge)
     *   triangle[r][r]   = 1              (right edge)
     *   triangle[r][c]   = triangle[r-1][c-1] + triangle[r-1][c]
     */
    memset(reference, 0, sizeof(reference));
    reference[0][0] = 1;
    for (int r = 1; r < NUM_ROWS; r++) {
        reference[r][0] = 1;
        for (int c = 1; c < r; c++)
            reference[r][c] = reference[r-1][c-1] + reference[r-1][c];
        reference[r][r] = 1;
    }
}

/* ── Verify parallel result against reference ─────────── */
int verify()
{
    cell_t *base = use_replication ? triangle_nodes[0] : triangle_shared[0]

    for (int r = 0; r < NUM_ROWS; r++)
        for (int c = 0; c <= r; c++)
            if (*tri(base, r, c) != reference[r][c]) {
                printf("  MISMATCH at row=%d col=%d "
                       "got=%lu expected=%lu\n",
                       r, c, *tri(base, r, c), reference[r][c]);
                return 0;
            }
    return 1;
}

/* ── Thread arguments ───────────────────────────────────── */
typedef struct {
    int thread_id;
    int num_threads;
    int node_id;
} ThreadArg;

void pin_thread(int node) {
    struct bitmask *cpus = numa_allocate_cpumask();

    numa_node_to_cpus(node, cpus);

    for (int i = 0; i < cpus->size; i++) {
        if (numa_bitmask_isbitset(cpus, i)) {
            cpu_set_t mask;
            CPU_ZERO(&mask);
            CPU_SET(i, &mask);
            sched_setaffinity(0, sizeof(mask), mask);
            break;
        }
    }

    numa_free_cpumask(cpus);
}

/* ── Worker: each thread runs this ──────────────────────── */
void *worker(void *arg)
{
    ThreadArg *t = (ThreadArg *)arg;

    pin_thread(t->node_id);

    cell_t *local = use_replication ? triangle_nodes[t->node_id] : triangle_shared;

    /*
     * Seed row 0: only thread 0 sets it.
     * All other threads skip this — they go straight to barrier.
     * The barrier then ensures all threads see row 0 before
     * any of them start row 1.
     */
    if (t->thread_id == 0)
        tri*(local, 0, 0) = 1;

    /* All threads wait here until row 0 is seeded */
    pthread_barrier_wait(&row_barrier);

    /* Compute rows 1 through NUM_ROWS-1 */
    for (int row = 1; row < NUM_ROWS; row++) {
        int row_len = row + 1;   /* row N has N+1 cells */

        /*
         * Divide this row's columns evenly among threads.
         *
         * Example: row has 100 cells, 4 threads:
         *   thread 0 → cols  0..24
         *   thread 1 → cols 25..49
         *   thread 2 → cols 50..74
         *   thread 3 → cols 75..99
         *
         * No two threads write the same column — no write conflict.
         */
        int chunk = (row_len + t->num_threads - 1) / t->num_threads;
        int start = t->thread_id * chunk;
        int end   = start + chunk;
        if (end > row_len) end = row_len;   /* clamp last thread */
        
        for (int col = start; col < end; col++) {
            cell_t val = (col == 0 || col == row) ?
                1 :
                *tri(local,row-1,col-1) + *tri(local,row-1,col);
            
            *tri(local, row, col) = val

            if (use_replication) {
                for (int n = 0; n < numa_nodes; n++) {
                    if (n == t->node_id)
                        continue;
                    *tri(triangle_nodes, row, col) = val;
                }
            }
            // if (col == 0 || col == row) {
            //     /* Left and right edges are always 1 */
            //     *tri(local,row,col) = 1;
            // } else {
            //     /*
            //      * Inner cell = left parent + right parent.
            //      * This READ from row-1 is the contention point.
            //      * In disaggregated memory: row-1 might be on a
            //      * remote machine — expensive network fetch.
            //      * NR gives each node a local cached copy of row-1.
            //      */
            //     *tri(local,row,col) = 
                                   
                
            // }
        }

        /*
         * BARRIER: every thread must finish its chunk of this row
         * before ANY thread can start reading row for the next row.
         * Without this, a fast thread might read an incomplete row.
         */
        pthread_barrier_wait(&row_barrier);
    }

    return NULL;
}

/* ── Run benchmark with N threads ───────────────────────── */
void run(int num_threads, FILE *csv)
{
    /* Clear triangle before each run */
    if (use_replication)
        for (int n = 0; n < numa_nodes; n++) 
            memset(triangle_nodes[n], 0, NUM_ROWS * MAX_COLS * sizeof(cell_t));
    else
        memset(triangle_shared, 0, NUM_ROWS * MAX_COLS * sizeof(cell_t));

    /* Create barrier for this run's thread count */
    pthread_barrier_init(&row_barrier, NULL, num_threads);

    pthread_t threads[MAX_THREADS];
    ThreadArg args[MAX_THREADS];

    /* Start timer */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /* Spawn threads */
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id   = i;
        args[i].num_threads = num_threads;
        args[i].node_id     = i % numa_nodes;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    /* Wait for all threads to finish */
    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    /* Stop timer */
    clock_gettime(CLOCK_MONOTONIC, &end);

    pthread_barrier_destroy(&row_barrier);

    /* ── Calculate results ── */
    double wall_sec  = (end.tv_sec  - start.tv_sec) +
                       (end.tv_nsec - start.tv_nsec) / 1e9;
    double rows_sec  = NUM_ROWS / wall_sec;
    int    correct   = verify();

    /* Print result */
    printf("  threads=%2d  time=%7.3fs  rows/sec=%8.0f  correct=%s\n",
           num_threads,
           wall_sec,
           rows_sec,
           correct ? "YES ✓" : "NO ✗ <-- BUG");

    /* Save to CSV */
    fprintf(csv, "%d,%.4f,%.0f,%s\n",
            num_threads, wall_sec, rows_sec,
            correct ? "YES" : "NO");
}

void allocate_triangles()
{
    size_t size =
        NUM_ROWS * MAX_COLS * sizeof(cell_t);

    if(use_replication)
    {
        triangle_nodes =
            malloc(sizeof(cell_t*)*numa_nodes);

        for(int n=0;n<numa_nodes;n++)
        {
            triangle_nodes[n] =
                numa_alloc_onnode(size,n);

            memset(triangle_nodes[n],0,size);
        }
    }
    else
    {
        triangle_shared =
            numa_alloc_onnode(size,0);

        memset(triangle_shared,0,size);
    }
}

/* ── Main ───────────────────────────────────────────────── */
int main()
{
    /* Check NUMA */
	use_replication = (argc > 1 && strcmp(argv[1], "-r") == 0);
	if (numa_available() != -1)
		numa_nodes = numa_max_node() + 1;

    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║         PASCAL'S TRIANGLE BENCHMARK                 ║\n");
    printf("║  NUMA nodes : %-3d                                   ║\n",
           numa_nodes);
    printf("║  Rows       : %-4d                                  ║\n",
           NUM_ROWS);
    printf("║  Goal: correct=YES proves parallel logic is right   ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* Compute reference once — used for correctness check */
    printf("  Computing reference (single-threaded)...\n\n");
    compute_reference();

    /* Open CSV */
    // mkdir("results", 0755);
    char* csv_name = replication ? "results/pascal_w_replication.csv" : "results/pascal_wo_replication.csv";
	FILE *csv = fopen(csv_name, "w");
    if (!csv) { perror("Cannot open results file"); return 1; }
    fprintf(csv, "threads,wall_sec,rows_per_sec,correct\n");

    printf("  %-10s %-12s %-12s %s\n",
           "Threads", "Time(s)", "Rows/sec", "Correct");
    printf("  %s\n", "──────────────────────────────────────────────");

    /* Run with increasing thread counts */
    allocate_triangles();
    int counts[] = {1, 2, 4, 8, 16};
    for (int i = 0; i < 5; i++)
        run(counts[i], csv);

    fclose(csv);

    printf("\n  Results saved to results/pascal_results.csv\n");
    printf("\n  What to look for:\n");
    printf("  → correct=YES for all runs = parallel logic works\n");
    printf("  → time changes show barrier synchronisation cost\n");
    printf("  → NR version will remove cross-node reads of prev row\n\n");

    return 0;
}
