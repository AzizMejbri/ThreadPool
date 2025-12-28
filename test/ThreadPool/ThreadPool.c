
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
 * Dummy tasks
 * ============================================================ */

static void *dummy_task(void *arg) {
    atomic_fetch_add((_Atomic int *)arg, 1);
    return NULL;
}

static void *delayed_task(void *arg) {
    int delay_ms = *(int *)arg;
    struct timespec ts = {delay_ms / 1000, (delay_ms % 1000) * 1000000};
    nanosleep(&ts, NULL);
    return NULL;
}

#define ENTRY(task, arg) ((void *)(arg))

/* ============================================================
 * 1. Initialization
 * ============================================================ */

void test_initialization(void) {
    printf("\n=== Test 1: Initialization ===\n");

    ThreadPool tp;
    ThreadPool_init(&tp, 4, THREADPOOL_STATIC);
    test(tp.threads != NULL);
    test(tp.taskQueue != NULL);
    test(tp.exit_status == false);

    ThreadPool_shutdown(&tp);
    summary();
}

/* ============================================================
 * 2. Single-thread execution
 * ============================================================ */

void test_single_thread_execution(void) {
    printf("\n=== Test 2: Single-thread execution ===\n");

    ThreadPool tp;
    ThreadPool_init(&tp, 1, THREADPOOL_STATIC);

    _Atomic int counter;
    atomic_init(&counter, 0);

    for (int i = 0; i < 100; i++) {
        ThreadPool_execute(&tp, dummy_task, &counter);
    }

    // wait for completion
    while (atomic_load(&tp.pending_tasks) > 0)
        sched_yield();

    test(atomic_load(&counter) == 100);
    ThreadPool_shutdown(&tp);
    summary();
}

/* ============================================================
 * 3. Multi-thread execution
 * ============================================================ */

void test_multi_thread_execution(void) {
    printf("\n=== Test 3: Multi-thread execution ===\n");

    ThreadPool tp;
    ThreadPool_init(&tp, 8, THREADPOOL_STATIC);

    _Atomic int counter;
    atomic_init(&counter, 0);

    const int N = 10000;
    for (int i = 0; i < N; i++)
        ThreadPool_execute(&tp, dummy_task, &counter);

    while (atomic_load(&tp.pending_tasks) > 0)
        sched_yield();

    test(atomic_load(&counter) == N);
    ThreadPool_shutdown(&tp);
    summary();
}

/* ============================================================
 * 4. Delayed / fuzzed tasks
 * ============================================================ */

void test_fuzzed_tasks(void) {
    printf("\n=== Test 4: Fuzzed tasks ===\n");

    ThreadPool tp;
    ThreadPool_init(&tp, 6, THREADPOOL_STATIC);

    srand((unsigned)time(NULL));
    const int N = 200;
    int delays[N];
    for (int i = 0; i < N; i++) {
        delays[i] = rand() % 50;
        ThreadPool_execute(&tp, delayed_task, &delays[i]);
    }

    // wait for completion
    usleep(200 * 1000);
    test(atomic_load(&tp.pending_tasks) == 0);
    ThreadPool_shutdown(&tp);
    summary();
}

/* ============================================================
 * 5. Stress test
 * ============================================================ */

void test_stress(void) {
    printf("\n=== Test 5: Stress ===\n");

    ThreadPool tp;
    ThreadPool_init(&tp, 8, THREADPOOL_STATIC);

    _Atomic int counter;
    atomic_init(&counter, 0);

    const int N = 100000;
    for (int i = 0; i < N; i++)
        ThreadPool_execute(&tp, dummy_task, &counter);

    while (atomic_load(&tp.pending_tasks) > 0)
        sched_yield();

    test(atomic_load(&counter) == N);
    ThreadPool_shutdown(&tp);
    summary();
}

/* ============================================================
 * 6. Performance benchmark
 * ============================================================ */

void test_performance(void) {
    printf("\n=== Test 6: Performance benchmark ===\n");

    ThreadPool tp;
    ThreadPool_init(&tp, 8, THREADPOOL_STATIC);

    _Atomic int counter;
    atomic_init(&counter, 0);

    const int N = 1000000;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < N; i++)
        ThreadPool_execute(&tp, dummy_task, &counter);

    while (atomic_load(&tp.pending_tasks) > 0)
        sched_yield();

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Executed %d tasks in %.3f s | Throughput: %.2f tasks/sec\n",
           N, elapsed, N / elapsed);

    test(atomic_load(&counter) == N);
    ThreadPool_shutdown(&tp);
    summary();
}

/* ============================================================
 * main
 * ============================================================ */

int main(void) {
    printf("Starting ThreadPool tests\n");
    printf("=========================\n");

    test_initialization();
    test_single_thread_execution();
    test_multi_thread_execution();
    test_fuzzed_tasks();
    test_stress();
    test_performance();

    printf("\n✓ All ThreadPool tests completed\n");
    return 0;
}
