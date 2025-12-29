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

#define TQ_EMPTY(thp)                                                          \
  (atomic_load_explicit(&thp->pending_tasks, memory_order_acquire) == 0)

#define IDLE_TIMEOUT_NS 1e9; // 1 second, adjust as needed

typedef struct {
  void *master; // void* <== ThreadPool*
  uint16_t id;
} WorkerThreadArgs;

typedef enum {
  THREADPOOL_CACHED = 0,
  THREADPOOL_STATIC = 1,
} ThreadPoolType;

typedef struct {
  pthread_t *threads;
  TaskQueue *taskQueue;
  WorkerThreadArgs *wtas;
  pthread_mutex_t sleep_mutex;
  pthread_cond_t sleep_cond;
  _Atomic(uint64_t) pending_tasks;
  _Atomic(uint16_t) thread_n;
  bool exit_status;
  ThreadPoolType thp_type;
} ThreadPool;

void ThreadPool_init(ThreadPool *thp, unsigned int thread_num, int flags);
bool ThreadPool_execute(ThreadPool *thp, Task task, Args args);
// TODO: implement threadpool with timeout
void ThreadPool_execute_with_timeout(ThreadPool *thp, Task task, Args args,
                                     unsigned timeout);
void ThreadPool_shutdown(ThreadPool *thp);

#endif
