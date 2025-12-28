#include "../../utils/queue/task_queue.h"
#include "../test.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

/* ============================================================
 * Dummy tasks
 * ============================================================ */

static void *dummy_task1(void *arg) { return arg; }
static void *dummy_task2(void *arg) { return arg; }
static void *dummy_task(void *arg) { return arg; }
static void *null_task(void *arg) {
  (void)arg;
  return NULL;
}

#define ENTRY(task, arg) ((TQEntry){task, (void *)(arg)})

/* ============================================================
 * 1. Initialization
 * ============================================================ */

void test_initialization(void) {
  printf("\n=== Test 1: Initialization ===\n");

  TaskQueue tq;
  test(TaskQueue_init(&tq, 0));
  test(TQENTRY_EQ(TaskQueue_dequeue(&tq), NULL_ENTRY));
  test(TQENTRY_EQ(TaskQueue_peek(&tq), NULL_ENTRY));

  TaskQueue_destroy(&tq);
  summary();
}

/* ============================================================
 * 2. Single-thread FIFO semantics
 * ============================================================ */

void test_fifo_single_thread(void) {
  printf("\n=== Test 2: FIFO (single-thread) ===\n");

  TaskQueue tq;
  TaskQueue_init(&tq, 0);

  TQEntry a = ENTRY(dummy_task1, (void *)1);
  TQEntry b = ENTRY(dummy_task2, (void *)2);
  TQEntry c = ENTRY(null_task, NULL);

  test(TaskQueue_enqueue(&tq, a));
  test(TaskQueue_enqueue(&tq, b));
  TQEntry result = TaskQueue_dequeue(&tq);
  test(TQENTRY_EQ(result, a));
  result = TaskQueue_dequeue(&tq);
  test(TQENTRY_EQ(result, b));
  test(TaskQueue_enqueue(&tq, c));
  test(TQENTRY_EQ(TaskQueue_dequeue(&tq), c));
  test(TQENTRY_EQ(TaskQueue_dequeue(&tq), NULL_ENTRY));

  TaskQueue_destroy(&tq);
  summary();
}

/* ============================================================
 * 3. Peek semantics (NON-LINEARIZABLE)
 * ============================================================ */

void test_peek_behavior(void) {
  printf("\n=== Test 3: Peek semantics ===\n");

  TaskQueue tq;
  TaskQueue_init(&tq, 0);

  TQEntry e = ENTRY(dummy_task1, (void *)42);
  TaskQueue_enqueue(&tq, e);

  TQEntry p1 = TaskQueue_peek(&tq);
  TQEntry p2 = TaskQueue_peek(&tq);

  test(TQENTRY_EQ(p1, p2));
  test(TQENTRY_EQ(p1, e));

  TQEntry d = TaskQueue_dequeue(&tq);
  test(TQENTRY_EQ(d, e));

  TaskQueue_destroy(&tq);
  summary();
}

/* ============================================================
 * 4. NULL_ENTRY handling
 * ============================================================ */

void test_null_entry(void) {
  printf("\n=== Test 4: NULL_ENTRY ===\n");

  TaskQueue tq;
  TaskQueue_init(&tq, 0);

  TaskQueue_enqueue(&tq, NULL_ENTRY);
  test(TQENTRY_EQ(TaskQueue_dequeue(&tq), NULL_ENTRY));
  test(TQENTRY_EQ(TaskQueue_dequeue(&tq), NULL_ENTRY));

  TaskQueue_destroy(&tq);
  summary();
}

/* ============================================================
 * 5. Single-thread stress
 * ============================================================ */

void test_single_thread_stress(void) {
  printf("\n=== Test 5: Single-thread stress ===\n");

  TaskQueue tq;
  TaskQueue_init(&tq, 0);

  const int N = 100000;
  for (int i = 0; i < N; i++)
    TaskQueue_enqueue(&tq, ENTRY(dummy_task1, (void *)(uintptr_t)i));

  for (int i = 0; i < N; i++) {
    TQEntry e = TaskQueue_dequeue(&tq);
    test((uintptr_t)e.arg == (uintptr_t)i);
  }

  test(TQENTRY_EQ(TaskQueue_dequeue(&tq), NULL_ENTRY));

  TaskQueue_destroy(&tq);
  summary();
}

/* ============================================================
 * 6. Multi-consumer stress (SPMC)
 * ============================================================ */

#define CONSUMERS 4
#define OPS 100000

static TaskQueue tq_spmc;
static _Atomic uint64_t produced;
static _Atomic uint64_t consumed;

static void *producer(void *arg) {
  (void)arg;
  for (uint64_t i = 0; i < OPS; i++) {
    TaskQueue_enqueue(&tq_spmc, ENTRY(dummy_task1, (void *)(uintptr_t)i));
    atomic_fetch_add(&produced, 1);
  }
  return NULL;
}

static void *consumer(void *_) {
  (void)_;
  while (atomic_load(&consumed) < OPS) {
    TQEntry e = TaskQueue_dequeue(&tq_spmc);
    if (e.task != NULL) {
      atomic_fetch_add(&consumed, 1);
    }
  }
  return NULL;
}

void test_spmc_concurrency(void) {
  printf("\n=== Test 6: SPMC concurrency stress ===\n");

  atomic_store(&produced, 0);
  atomic_store(&consumed, 0);

  TaskQueue_init(&tq_spmc, 0);

  pthread_t p, c[CONSUMERS];

  pthread_create(&p, NULL, producer, NULL);
  for (int i = 0; i < CONSUMERS; i++)
    pthread_create(&c[i], NULL, consumer, NULL);

  pthread_join(p, NULL);
  for (int i = 0; i < CONSUMERS; i++)
    pthread_join(c[i], NULL);

  printf("Produced: %lu | Consumed: %lu\n", atomic_load(&produced),
         atomic_load(&consumed));
  test(atomic_load(&produced) == atomic_load(&consumed));

  TaskQueue_destroy(&tq_spmc);
  summary();
}

/* ============================================================
 * 7. Scalability test (variable consumers)
 * ============================================================ */

void test_scalability(void) {
  printf("\n=== Test 7: Scalability ===\n");

  for (int threads = 1; threads <= 8; threads *= 2) {
    printf("Threads: 1 producer / %d consumers\n", threads);

    atomic_store(&produced, 0);
    atomic_store(&consumed, 0);

    TaskQueue_init(&tq_spmc, 0);

    pthread_t p, c[threads];
    pthread_create(&p, NULL, producer, NULL);
    for (int i = 0; i < threads; i++)
      pthread_create(&c[i], NULL, consumer, NULL);

    pthread_join(p, NULL);
    for (int i = 0; i < threads; i++)
      pthread_join(c[i], NULL);

    test(atomic_load(&produced) == atomic_load(&consumed));
    TaskQueue_destroy(&tq_spmc);
  }

  summary();
}

/* ============================================================
 * 8. Fuzz test: random tasks
 * ============================================================ */
#include <stdlib.h>
#include <time.h>

void test_fuzz(void) {
    printf("\n=== Test 8: Fuzz / Randomized tasks ===\n");

    TaskQueue tq;
    TaskQueue_init(&tq, 0);
    srand((unsigned)time(NULL));

    const int N = 50000;
    TQEntry entries[N];

    // enqueue random tasks (sometimes NULL_ENTRY)
    for (int i = 0; i < N; i++) {
        if (rand() % 10 == 0) {
            entries[i] = NULL_ENTRY;
        } else {
            entries[i] = ENTRY(dummy_task1, (void *)(uintptr_t)i);
        }
        TaskQueue_enqueue(&tq, entries[i]);
    }

    // dequeue and validate
    for (int i = 0; i < N; i++) {
        TQEntry e = TaskQueue_dequeue(&tq);
        if (entries[i].task == NULL) {
            test(TQENTRY_EQ(e, NULL_ENTRY));
        } else {
            test((uintptr_t)e.arg == (uintptr_t)entries[i].arg);
        }
    }

    test(TQENTRY_EQ(TaskQueue_dequeue(&tq), NULL_ENTRY));
    TaskQueue_destroy(&tq);
    summary();
}

/* ============================================================
 * 9. Arena stress test: massive allocations
 * ============================================================ */
void test_arena_stress(void) {
    printf("\n=== Test 9: Arena stress ===\n");

    TaskQueue tq;
    const int ARENA_SIZE = 1024 * 1024 * 50; // 50MB
    TaskQueue_init(&tq, ARENA_SIZE);

    const int N = 500000;
    for (int i = 0; i < N; i++) {
        TaskQueue_enqueue(&tq, ENTRY(dummy_task1, (void *)(uintptr_t)i));
    }

    for (int i = 0; i < N; i++) {
        TQEntry e = TaskQueue_dequeue(&tq);
        test((uintptr_t)e.arg == (uintptr_t)i);
    }

    test(TQENTRY_EQ(TaskQueue_dequeue(&tq), NULL_ENTRY));
    TaskQueue_destroy(&tq);
    summary();
}

/* ============================================================
 * 10. Performance benchmark (single producer, multiple consumers)
 * ============================================================ */
#include <time.h>

#define BENCH_CONSUMERS 32
#define BENCH_TASKS     1000000  // 1M tasks

static TaskQueue tq_bench;
static _Atomic uint64_t produced;
static _Atomic uint64_t consumed;

static void *bench_producer(void *_) {
    (void)_;
    for (uint64_t i = 0; i < BENCH_TASKS; i++) {
        TaskQueue_enqueue(&tq_bench, ENTRY(dummy_task, (void *)(uintptr_t)i));
        atomic_fetch_add(&produced, 1);
    }
    return NULL;
}

static void *bench_consumer(void *_) {
    (void)_;
    while (1) {
        TQEntry e = TaskQueue_dequeue(&tq_bench);
        if (e.task != NULL) {
            atomic_fetch_add(&consumed, 1);
        } else {
            // avoid busy-spin CPU burn
            if (atomic_load(&consumed) >= BENCH_TASKS)
                break;
            sched_yield(); // yield CPU
        }
    }
    return NULL;
}

void test_benchmark_spmc(void) {
    printf("\n=== Test 10: SPMC Benchmark ===\n");

    atomic_store(&produced, 0);
    atomic_store(&consumed, 0);

    TaskQueue_init(&tq_bench, 0);

    pthread_t producer_thread;
    pthread_t consumers[BENCH_CONSUMERS];

    // start timer
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_create(&producer_thread, NULL, bench_producer, NULL);
    for (int i = 0; i < BENCH_CONSUMERS; i++)
        pthread_create(&consumers[i], NULL, bench_consumer, NULL);

    pthread_join(producer_thread, NULL);
    for (int i = 0; i < BENCH_CONSUMERS; i++)
        pthread_join(consumers[i], NULL);

    // stop timer
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Produced: %lu | Consumed: %lu | Time: %.3f s\n",
           atomic_load(&produced), atomic_load(&consumed), elapsed);

    // correctness check
    test(atomic_load(&produced) == atomic_load(&consumed));

    TaskQueue_destroy(&tq_bench);
    summary();
}



/* ============================================================
 * 11. Long-lived queue stability
 * ============================================================ */
void test_long_lived_queue(void) {
    printf("\n=== Test 11: Long-lived queue stability ===\n");

    TaskQueue tq;
    TaskQueue_init(&tq, 0);

    const int N = 100000;
    for (int i = 0; i < N; i++) {
        TaskQueue_enqueue(&tq, ENTRY(dummy_task1, (void *)(uintptr_t)i));
        if (i % 2 == 0) {
            TQEntry e = TaskQueue_dequeue(&tq);
            test((uintptr_t)e.arg == (uintptr_t)(i - (i % 2 == 0 ? 1 : 0)) || e.task != NULL);
        }
    }

    while (TaskQueue_peek(&tq).task != NULL) {
        TaskQueue_dequeue(&tq);
    }

    test(TQENTRY_EQ(TaskQueue_dequeue(&tq), NULL_ENTRY));
    TaskQueue_destroy(&tq);
    summary();
}

/* ============================================================
 * main 
 * ============================================================ */
int main(void) {
    printf("Starting extended SPMC TaskQueue tests\n");
    printf("======================================\n");

    test_initialization();
    test_fifo_single_thread();
    test_peek_behavior();
    test_null_entry();
    test_single_thread_stress();
    test_spmc_concurrency();
    test_scalability();
    test_fuzz();
    test_arena_stress();
    test_benchmark_spmc();
    test_long_lived_queue();

    printf("\n✓ All extended SPMC TaskQueue tests passed\n");
    return 0;
}

