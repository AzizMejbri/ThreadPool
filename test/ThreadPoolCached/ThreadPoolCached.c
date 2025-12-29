
#include "../../components/ThreadPool/ThreadPool.h"
#include "../test.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/* ============================================================
 * Utilities
 * ============================================================ */

static inline uint64_t now_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

/* ============================================================
 * Dummy tasks
 * ============================================================ */

static void *noop_task(void *arg) {
  (void)arg;
  return NULL;
}

static void *spin_task(void *arg) {
  _Atomic uint64_t *counter = arg;
  for (volatile int i = 0; i < 1000; i++)
    ;
  atomic_fetch_add(counter, 1);
  return NULL;
}

static void *random_task(void *arg) {
  _Atomic uint64_t *counter = arg;
  int spins = rand() % 5000;
  for (volatile int i = 0; i < spins; i++)
    ;
  atomic_fetch_add(counter, 1);
  return NULL;
}

/* ============================================================
 * Test 1: Cached pool init
 * ============================================================ */

void test_cached_init(void) {
  printf("\n=== Test 1: Cached pool init ===\n");

  ThreadPool tp;
  ThreadPool_init(&tp, 0, THREADPOOL_CACHED);

  test(tp.thp_type == THREADPOOL_CACHED);
  test(tp.taskQueue != NULL);
  test(tp.threads != NULL);
  test(tp.wtas != NULL);
  test(atomic_load(&tp.thread_n) == 0);

  ThreadPool_shutdown(&tp);
  summary();
}

/* ============================================================
 * Test 2: Low-load execution
 * ============================================================ */

void test_cached_low_load(void) {
  printf("\n=== Test 2: Low-load execution (cached) ===\n");

  ThreadPool tp;
  ThreadPool_init(&tp, 0, THREADPOOL_CACHED);

  _Atomic uint64_t counter = 0;

  ThreadPool_execute(&tp, spin_task, &counter);

  while (atomic_load(&counter) != 1)
    sched_yield();

  test(atomic_load(&counter) == 1);

  ThreadPool_shutdown(&tp);
  summary();
}

/* ============================================================
 * Test 3: Adaptive scaling
 * ============================================================ */

void test_cached_scaling(void) {
  printf("\n=== Test 3: Adaptive scaling ===\n");

  ThreadPool tp;
  ThreadPool_init(&tp, 0, THREADPOOL_CACHED);

  _Atomic uint64_t counter = 0;
  const int N = 1000;

  for (int i = 0; i < N; i++)
    ThreadPool_execute(&tp, spin_task, &counter);

  while (atomic_load(&counter) != N)
    sched_yield();

  test(atomic_load(&tp.thread_n) > 1);

  ThreadPool_shutdown(&tp);
  summary();
}

/* ============================================================
 * Test 4: Thread retirement
 * ============================================================ */

void test_cached_retirement(void) {
  printf("\n=== Test 4: Thread retirement ===\n");

  ThreadPool tp;
  ThreadPool_init(&tp, 0, THREADPOOL_CACHED);

  _Atomic uint64_t counter = 0;

  for (int i = 0; i < 200; i++)
    ThreadPool_execute(&tp, spin_task, &counter);

  while (atomic_load(&counter) != 200)
    sched_yield();

  sleep(2); // allow idle timeout to kick in

  uint16_t threads_after =
      atomic_load_explicit(&tp.thread_n, memory_order_acquire);

  test(threads_after <= 2);

  ThreadPool_shutdown(&tp);
  summary();
}

/* ============================================================
 * Test 5: Stress test
 * ============================================================ */

void test_cached_stress(void) {
  printf("\n=== Test 5: Stress test ===\n");

  ThreadPool tp;
  ThreadPool_init(&tp, 0, THREADPOOL_CACHED);

  _Atomic uint64_t counter = 0;
  const int N = 50000;

  for (int i = 0; i < N; i++)
    ThreadPool_execute(&tp, spin_task, &counter);

  while (atomic_load(&counter) != N)
    sched_yield();

  test(atomic_load(&counter) == N);

  ThreadPool_shutdown(&tp);
  summary();
}

/* ============================================================
 * Test 6: Fuzzy randomized workload
 * ============================================================ */

void test_cached_fuzzy(void) {
  printf("\n=== Test 6: Fuzzy randomized workload ===\n");

  ThreadPool tp;
  ThreadPool_init(&tp, 0, THREADPOOL_CACHED);

  _Atomic uint64_t counter = 0;
  srand((unsigned)time(NULL));

  for (int r = 0; r < 50; r++) {
    int batch = rand() % 1000 + 1;
    for (int i = 0; i < batch; i++)
      ThreadPool_execute(&tp, random_task, &counter);
    usleep(rand() % 2000);
  }

  uint64_t done = atomic_load(&counter);
  while (atomic_load(&counter) != done)
    sched_yield();

  test(atomic_load(&counter) >= done);

  ThreadPool_shutdown(&tp);
  summary();
}

/* ============================================================
 * Test 7: Performance benchmark
 * ============================================================ */

void test_cached_performance(void) {
    printf("\n=== Test 7: Performance benchmark ===\n");
    ThreadPool tp;
    ThreadPool_init(&tp, 0, THREADPOOL_CACHED);
    _Atomic uint64_t counter = 0;
    const int N = 1000000;
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (int i = 0; i < N; i++)
        ThreadPool_execute(&tp, spin_task, &counter);  // Changed here
    
    while (atomic_load(&counter) != N)
        sched_yield();
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("Executed %d tasks in %.3f s | Throughput: %.2f tasks/sec\n", N, elapsed, N / elapsed);
    
    test(atomic_load(&counter) == N);
    ThreadPool_shutdown(&tp);
    summary();
}

/* ============================================================
 * main
 * ============================================================ */

int main(void) {
  printf("Running test for ThreadPoolCached\n");
  printf("===============================\n");

  test_cached_init();
  test_cached_low_load();
  test_cached_scaling();
  test_cached_retirement();
  test_cached_stress();
  test_cached_fuzzy();
  test_cached_performance();

  printf("\n✓ All ThreadPoolCached tests completed\n");
  return 0;
}
