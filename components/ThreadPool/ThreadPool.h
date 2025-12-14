#ifndef THREADPOOL_H
#define THREADPOOL_H


#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>


// Throughput = N / (1 + α(N-1) + βN(N-1))
// Where:
//   N = # of threads
//   α = Contention (waiting for locks/resources)
//   β = Coherency (cache invalidation overhead)
//
// As N increases beyond cores:
//   β dominates → throughput decreases


typedef void* (*Task)(void*);
typedef void* Args; 

typedef struct {
  pthread_t *threads; 
  Task*       tasks;
  Args*       args;
  bool*       busy_status;
  bool*       exit_status;
  pthread_mutex_t available_thread_mutex;
  pthread_cond_t available_thread_cond;
  pthread_mutex_t *available_task_mutex;
  pthread_cond_t *available_task_cond;
  int32_t    ready_tid_n;
  uint16_t   thread_n;
} ThreadPool;

typedef enum {
  THREADPOOL_CACHED = 0,
  THREADPOOL_STATIC = 1, 
} ThreadPoolType;

void ThreadPool_init(ThreadPool *thp, unsigned int thread_num, int flags);
void ThreadPool_execute(ThreadPool *thp, Task task, Args args);
void ThreadPool_shutdown(ThreadPool *thp);

#endif
