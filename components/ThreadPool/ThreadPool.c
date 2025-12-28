#include "ThreadPool.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "../../utils/cores.h"
#include "../../utils/timeout/timeout.h"

static inline void ThreadPoolStatic_init(ThreadPool *thp, unsigned int num);
static inline void ThreadPoolCached_init(ThreadPool *thp, unsigned int num);

/*
 *
 * =======================================================================
 *  Static Thread fn
 * =======================================================================
 *
 *
 */

static void *thread_static_fn(void *_args) {
  TQEntry e;
  WorkerThreadArgs *args;
  args = (WorkerThreadArgs *)_args;

static_thread_work_wait:
  // if it recieves an exit signal it exits, else it consumes a task if existing
  // and executes it
  pthread_mutex_lock(&((ThreadPool *)args->master)->sleep_mutex);
  while (!((ThreadPool *)args->master)->exit_status &&
         TQ_EMPTY(((ThreadPool *)args->master))) {
    pthread_cond_wait(&((ThreadPool *)args->master)->sleep_cond,
                      &((ThreadPool *)args->master)->sleep_mutex);
  }
  if (((ThreadPool *)args->master)->exit_status)
    goto static_thread_work_exit;
  pthread_mutex_unlock(&((ThreadPool *)args->master)->sleep_mutex);

  // the work bulk, the thread consumes a task and executes it if it find one,
  // else it goes back to waiting
  e = TaskQueue_dequeue(((ThreadPool *)args->master)->taskQueue);

  // for its task and start executing our proper task
  if (!TQENTRY_EQ(e, NULL_ENTRY)) {
    atomic_fetch_sub_explicit(&((ThreadPool *)args->master)->pending_tasks, 1,
                              memory_order_release);
    e.task(e.arg);
  }

  // after executing our task, we go back to waiting for a task
  goto static_thread_work_wait;

  // if the thread receives an exit signal, it frees resources and returns,
  // preparing to be joined by the main thread
static_thread_work_exit:
  pthread_mutex_unlock(&((ThreadPool *)args->master)->sleep_mutex);
  return NULL;
}

/*
 *
 * =======================================================================
 *  Cached Thread fn
 * =======================================================================
 *
 *
 */

static void *thread_cached_fn(void *_args) {
  TQEntry e;
  WorkerThreadArgs *args;
  args = (WorkerThreadArgs *)_args;

cached_thread_work_wait:
  // if it recieves an exit signal it exits, else it consumes a task if existing
  // and executes it
  pthread_mutex_lock(&((ThreadPool *)args->master)->sleep_mutex);

  while (!((ThreadPool *)args->master)->exit_status &&
         TQ_EMPTY(((ThreadPool *)args->master))) {
    // setup absolute timeout for pthread_cond_timedwait
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += IDLE_TIMEOUT_NS;
    if (ts.tv_nsec >= 1000000000) { // normalize overflow
      ts.tv_sec += ts.tv_nsec / 1000000000;
      ts.tv_nsec = ts.tv_nsec % 1000000000;
    }

    int rc =
        pthread_cond_timedwait(&((ThreadPool *)args->master)->sleep_cond,
                               &((ThreadPool *)args->master)->sleep_mutex, &ts);
    if (rc == ETIMEDOUT) {
      // no tasks arrived during timeout → exit thread
      pthread_mutex_unlock(&((ThreadPool *)args->master)->sleep_mutex);
      goto cached_thread_work_exit;
    }
  }

  if (((ThreadPool *)args->master)->exit_status)
    goto cached_thread_work_exit;
  pthread_mutex_unlock(&((ThreadPool *)args->master)->sleep_mutex);

  // the work bulk, the thread consumes a task and executes it if it find one,
  // else it goes back to waiting
  e = TaskQueue_dequeue(((ThreadPool *)args->master)->taskQueue);

  // for its task and start executing our proper task
  if (!TQENTRY_EQ(e, NULL_ENTRY)) {
    atomic_fetch_sub_explicit(&((ThreadPool *)args->master)->pending_tasks, 1,
                              memory_order_release);
    e.task(e.arg);
  }

  // after executing our task, we go back to waiting for a task
  goto cached_thread_work_wait;

  // if the thread receives an exit signal, it frees resources and returns,
  // preparing to be joined by the main thread
cached_thread_work_exit:
  pthread_mutex_unlock(&((ThreadPool *)args->master)->sleep_mutex);
  atomic_fetch_sub_explicit(&((ThreadPool *)args->master)->thread_n, 1,
                            memory_order_release);
  return NULL;
}

void ThreadPool_init(ThreadPool *thp, unsigned int num, int flags) {
  if (flags == THREADPOOL_STATIC)
    ThreadPoolStatic_init(thp, num);
  else
    ThreadPoolCached_init(thp, num);
}

static inline void ThreadPoolStatic_init(ThreadPool *thp, unsigned int num) {

  if (num == 0)
    num = logical_cores_count();

  thp->thp_type = THREADPOOL_STATIC;
  thp->threads = malloc(sizeof(pthread_t) * num);
  thp->taskQueue = malloc(sizeof(TaskQueue));
  TaskQueue_init(thp->taskQueue, 1024);
  thp->thread_n = num;
  thp->exit_status = false;
  pthread_mutex_init(&thp->sleep_mutex, NULL);
  pthread_cond_init(&thp->sleep_cond, NULL);
  atomic_init(&thp->pending_tasks, 0);
  thp->wtas = malloc(sizeof(WorkerThreadArgs) * thp->thread_n);
  for (unsigned i = 0; i < num; i++) {
    thp->wtas[i].master = thp;
    thp->wtas[i].id = i;
    pthread_create(thp->threads + i, NULL, thread_static_fn, thp->wtas + i);
  }
}

static inline void ThreadPoolCached_init(ThreadPool *thp, unsigned int num) {

  thp->thp_type = THREADPOOL_CACHED;
  atomic_init(&thp->thread_n, num != 0 ? num : logical_cores_count());
  thp->threads = malloc(sizeof(pthread_t) * 4 * thp->thread_n);
  thp->taskQueue = malloc(sizeof(TaskQueue));
  TaskQueue_init(thp->taskQueue, 1024);
  thp->exit_status = false;
  pthread_mutex_init(&thp->sleep_mutex, NULL);
  pthread_cond_init(&thp->sleep_cond, NULL);
  atomic_init(&thp->pending_tasks, 0);
  thp->wtas = malloc(sizeof(WorkerThreadArgs) * thp->thread_n);
  for (unsigned i = 0; i < num; i++) {
    thp->wtas[i].master = thp;
    thp->wtas[i].id = i;
  }
}

static inline void ThreadPoolStatic_execute(ThreadPool *thp, Task task,
                                             Args args) {
  atomic_fetch_add_explicit(&thp->pending_tasks, 1, memory_order_release);
  TaskQueue_enqueue(thp->taskQueue, (TQEntry){task, args});

  pthread_mutex_lock(&thp->sleep_mutex);
  pthread_cond_signal(&thp->sleep_cond);
  pthread_mutex_unlock(&thp->sleep_mutex);
}

static inline void ThreadPoolCached_execute(ThreadPool *thp, Task task,
                                             Args args) {

  atomic_fetch_add_explicit(&thp->pending_tasks, 1, memory_order_release);

  uint16_t thread_n =
      atomic_load_explicit(&thp->thread_n, memory_order_acquire);
  if (thread_n < thp->pending_tasks && thread_n <= 4 * logical_cores_count()) {
    pthread_create(thp->threads + thread_n, NULL, thread_cached_fn,
                   thp->wtas + thread_n);
    atomic_fetch_add_explicit(&thp->thread_n, 1, memory_order_release);
  } else {
    TaskQueue_enqueue(thp->taskQueue, (TQEntry){task, args});

    pthread_mutex_lock(&thp->sleep_mutex);
    pthread_cond_signal(&thp->sleep_cond);
    pthread_mutex_unlock(&thp->sleep_mutex);
  }
}

bool ThreadPool_execute(ThreadPool *thp, Task task, Args args) {
  if (thp == NULL || thp->exit_status) {
    return false;
  }
  // lock the task queue mutex and notify all active_workers threads about the
  // addition
  switch (thp->thp_type) {
  case THREADPOOL_STATIC:
    ThreadPoolStatic_execute(thp, task, args);
    return true;
  case THREADPOOL_CACHED:
    ThreadPoolCached_execute(thp, task, args);
    return true;
  default:
    break;
  }
}

static inline void ThreadPoolStatic_shutdown(ThreadPool *thp) {

  pthread_mutex_lock(&thp->sleep_mutex);
  thp->exit_status = true;
  pthread_cond_broadcast(&thp->sleep_cond);
  pthread_mutex_unlock(&thp->sleep_mutex);
  uint16_t thread_n =
      atomic_load_explicit(&thp->thread_n, memory_order_acquire);
  for (unsigned i = 0; i < thread_n; i++) {
    pthread_join(thp->threads[i], NULL);
  }

  pthread_mutex_destroy(&thp->sleep_mutex);
  pthread_cond_destroy(&thp->sleep_cond);
  TaskQueue_destroy(thp->taskQueue);
  free(thp->taskQueue);
  free(thp->threads);
  free(thp->wtas);
}
static inline void ThreadPoolCached_shutdown(ThreadPool *thp) {}

void ThreadPool_shutdown(ThreadPool *thp) {
  switch (thp->thp_type) {
  case THREADPOOL_STATIC:
    return ThreadPoolStatic_shutdown(thp);
  case THREADPOOL_CACHED:
    return ThreadPoolCached_shutdown(thp);
  default:
    return;
  }
}
