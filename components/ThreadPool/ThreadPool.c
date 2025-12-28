#include "ThreadPool.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../utils/cores.h"
#include "../../utils/timeout/timeout.h"

#define TQ_EMPTY(thp)                                                          \
  (atomic_load_explicit(&thp->pending_tasks, memory_order_acquire) == 0)

typedef struct {
  ThreadPool *master;
  uint16_t id;
} WorkerThreadArgs;

static inline void ThreadPoolStatic_init(ThreadPool *thp, unsigned int num);
static inline void ThreadPoolCached_init(ThreadPool *thp);

static void *thread_fn(void *_args) {
  TQEntry e;
  WorkerThreadArgs *args;
  args = (WorkerThreadArgs *)_args;

thread_work_wait:
  // if it recieves an exit signal it exits, else it consumes a task if existing
  // and executes it
  pthread_mutex_lock(&args->master->sleep_mutex);
  while (!args->master->exit_status && TQ_EMPTY(args->master)) {
    pthread_cond_wait(&args->master->sleep_cond, &args->master->sleep_mutex);
  }
  if (args->master->exit_status)
    goto thread_work_exit;
  pthread_mutex_unlock(&args->master->sleep_mutex);

  // the work bulk, the thread consumes a task and executes it if it find one,
  // else it goes back to waiting
  e = TaskQueue_dequeue(args->master->taskQueue);

  // for its task and start executing our proper task
  if (!TQENTRY_EQ(e, NULL_ENTRY)) {
    atomic_fetch_sub_explicit(&args->master->pending_tasks, 1,
                              memory_order_release);
    e.task(e.arg);
  }

  // after executing our task, we go back to waiting for a task
  goto thread_work_wait;

  // if the thread receives an exit signal, it frees resources and returns,
  // preparing to be joined by the main thread
thread_work_exit:
  pthread_mutex_unlock(&args->master->sleep_mutex);
  free(args);
  return NULL;
}

void ThreadPool_init(ThreadPool *thp, unsigned int num, int flags) {
  if (flags == THREADPOOL_STATIC)
    ThreadPoolStatic_init(thp, num);
  else
    ThreadPoolCached_init(thp);
}

static inline void ThreadPoolStatic_init(ThreadPool *thp, unsigned int num) {

  if (num == 0)
    num = logical_cores_count();

  thp->threads = malloc(sizeof(pthread_t) * num);
  thp->taskQueue = malloc(sizeof(TaskQueue));
  TaskQueue_init(thp->taskQueue, 1024);
  thp->thread_n = num;
  thp->exit_status = false;
  pthread_mutex_init(&thp->sleep_mutex, NULL);
  pthread_cond_init(&thp->sleep_cond, NULL);
  atomic_init(&thp->pending_tasks, 0);
  for (unsigned i = 0; i < num; i++) {
    WorkerThreadArgs *wta = malloc(sizeof(WorkerThreadArgs));
    wta->master = thp;
    wta->id = i;
    pthread_create(thp->threads + i, NULL, thread_fn, wta);
  }
}

static inline void ThreadPoolCached_init(ThreadPool *thp) {
  // TODO: implemented a cached thread pool
  ThreadPoolStatic_init(thp, 0);
}

bool ThreadPool_execute(ThreadPool *thp, Task task, Args args) {
  if (thp == NULL || thp->exit_status) {
    return false;
  }
  // lock the task queue mutex and notify all active_workers threads about the
  // addition
  atomic_fetch_add_explicit(&thp->pending_tasks, 1, memory_order_release);
  TaskQueue_enqueue(thp->taskQueue, (TQEntry){task, args});

  pthread_mutex_lock(&thp->sleep_mutex);
  pthread_cond_signal(&thp->sleep_cond);
  pthread_mutex_unlock(&thp->sleep_mutex);
  return true;
}

void ThreadPool_shutdown(ThreadPool *thp) {
  pthread_mutex_lock(&thp->sleep_mutex);
  thp->exit_status = true;
  pthread_cond_broadcast(&thp->sleep_cond);
  pthread_mutex_unlock(&thp->sleep_mutex);
  for (unsigned i = 0; i < thp->thread_n; i++) {
    pthread_join(thp->threads[i], NULL);
  }

  pthread_mutex_destroy(&thp->sleep_mutex);
  pthread_cond_destroy(&thp->sleep_cond);
  TaskQueue_destroy(thp->taskQueue);
  free(thp->taskQueue);
  free(thp->threads);
}
