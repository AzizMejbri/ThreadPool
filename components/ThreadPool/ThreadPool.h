#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../../utils/queue/task_queue.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

// Throughput = N / (1 + α(N-1) + βN(N-1))
// Where:
//   N = # of threads
//   α = Contention (waiting for locks/resources)
//   β = Coherency (cache invalidation overhead)
//
// As N increases beyond cores:
//   β dominates → throughput decreases

typedef void *(*Task)(void *);
typedef void *Args;

typedef struct {
  pthread_t *threads;
  TaskQueue *taskQueue;
  pthread_mutex_t sleep_mutex;
  pthread_cond_t sleep_cond;
  _Atomic(uint64_t) pending_tasks;
  uint16_t thread_n;
  bool exit_status;
} ThreadPool;

typedef enum {
  THREADPOOL_CACHED = 0,
  THREADPOOL_STATIC = 1,
} ThreadPoolType;

void ThreadPool_init(ThreadPool *thp, unsigned int thread_num, int flags);
bool ThreadPool_execute(ThreadPool *thp, Task task, Args args);
void ThreadPool_execute_with_timeout(ThreadPool *thp, Task task, Args args,
                                     unsigned timeout);
void ThreadPool_shutdown(ThreadPool *thp);

#endif
