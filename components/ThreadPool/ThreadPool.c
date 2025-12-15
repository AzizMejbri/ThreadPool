#include "ThreadPool.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../utils/cores.h"

// void ThreadPool_init(ThreadPool *thp, unsigned int thread_num, int flags);
// void ThreadPool_execute(ThreadPool *thp, Task task, Args args);

// ThreadPool thp;
// ThreadPool_init(thp, 0, THREADPOOL_STATIC);
// while(task_queue.size != 0){
//  ThreadPool_execute(thp, task_queue.next, NULL);
// }
// ThreadPool_cleanup(thp);
//

#define set_bit(bitset, n) (bitset | (1 << n))
#define clr_bit(bitset, n) (bitset & ~(1 << n))
#define get_bit(bitset, n) (bitset >> n)

typedef struct {
  ThreadPool* master;
  uint16_t    id;
} WorkerThreadArgs;

static inline void ThreadPoolStatic_init(ThreadPool *thp, unsigned int num);
static inline void ThreadPoolCached_init(ThreadPool *thp);

static void *thread_fn(void *_args) { 
  WorkerThreadArgs *args ;
  args = (WorkerThreadArgs*)_args;

thread_work_wait:
  // wait for a task, if it recieves a signal that a task is provided it executes it 
  // FIX: otherwise if it recieved an exit signal, it exits
  
  pthread_mutex_lock(args -> master -> available_task_mutex + args -> id);

  while (args -> master -> tasks[args -> id] == NULL && !args -> master -> exit_status[args -> id])
    pthread_cond_wait(args -> master -> available_task_cond + args -> id, args -> master ->available_task_mutex + args -> id); 

  pthread_mutex_unlock(args -> master -> available_task_mutex + args -> id);

  if (args -> master -> exit_status[args -> id])
    goto thread_work_exit;
 

thread_work_start:
  // thread does its task
  // it toggles its busy status index, subtracts the ready_threads_num after locking its mutex, unlocks the mutex, does its task

  pthread_mutex_lock(&args -> master -> available_thread_mutex);
  args -> master -> busy_status[ args -> id ] = true;
  args -> master -> ready_tid_n --;
  pthread_mutex_unlock(&args -> master -> available_thread_mutex);

  // executes its task
  args -> master -> tasks[args -> id](args -> master -> args[args -> id]);

  // got its task done, adds to read_threads_num and toggles its busy index to signal its free, signals that a change has happened
  // to available_thread_cond for a blocking pool
  pthread_mutex_lock(args -> master -> available_task_mutex + args -> id);
  args -> master -> busy_status[args -> id] = false;
  args -> master -> ready_tid_n ++;
  pthread_cond_broadcast(&args -> master -> available_thread_cond);

  // now it sets its task to NULL, and waits until it is set otherwise
  args -> master -> tasks[args -> id] = NULL;

  // an exception is if its exit_status is true, the thread is forced to exit after achieving its task if the exit_status is set
  if ( args -> master -> exit_status[args -> id] )
    goto thread_work_exit;

 
  // at this point we recieved a signal that the task is no long set to NULL, we go to thread_work_start to restart execution
  goto thread_work_wait;

thread_work_exit:
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

  thp->thread_n = num;
  thp->ready_tid_n = num;
  thp->threads = malloc(sizeof(pthread_t) * num);
  thp->tasks = malloc(sizeof(Task) * num);
  thp->args = malloc(sizeof(Args) * num);
  thp->busy_status = malloc(sizeof(bool) * num);
  thp->exit_status = malloc(sizeof(bool) * num);
  thp->available_task_mutex = malloc(sizeof(pthread_mutex_t) * num);
  thp->available_task_cond = malloc(sizeof(pthread_cond_t) * num);
  pthread_mutex_init(&thp -> available_thread_mutex, NULL);
  pthread_cond_init(&thp -> available_thread_cond, NULL);
  for(unsigned i = 0; i < num; i++){
    thp -> busy_status[i] = false;
    thp -> exit_status[i] = false;
    pthread_mutex_init(thp -> available_task_mutex + i, NULL);
    pthread_cond_init(thp -> available_task_cond + i, NULL);
  }
  for (unsigned i = 0; i < num; i++) {
    WorkerThreadArgs *wta = malloc(sizeof(WorkerThreadArgs));
    wta -> master = thp;
    wta -> id = i;
    pthread_create(thp->threads + i, NULL, thread_fn, wta);
  }
}

static inline void ThreadPoolCached_init(ThreadPool *thp) {
  // TODO: implemented a cached thread pool
  return ThreadPoolStatic_init(thp, 0);
}


static inline int32_t poll_threads(bool *busy_status, uint16_t n){
  for(uint16_t i = 0; i < n; i++)
    if ( !busy_status[i] )
      return i;
  return -1; 
}
void ThreadPool_execute(ThreadPool *thp, Task task, Args args){
  int32_t poll_threads_out;
poll:
  // we poll the threads to see if any is available 
  pthread_mutex_lock(&thp -> available_thread_mutex);
  poll_threads_out = poll_threads(thp -> busy_status, thp -> thread_n);
  pthread_mutex_unlock(&thp -> available_thread_mutex);

  // case 1: no thread is available: we wait until one is, when a thread finished execution and before sleeping
  // it increments the pool's read_tid_n and broadcasts a signal that will reawaken us to recheck the status of 
  // the pool's read_tid_n, if it is no longer 0 we go back to the beginning and poll the threads again to see 
  // which one is available
  if ( thp -> ready_tid_n == 0 ){
    pthread_mutex_lock(&thp -> available_thread_mutex);
    while ( thp -> ready_tid_n == 0 ){
      pthread_cond_wait(&thp -> available_thread_cond, &thp -> available_thread_mutex);
    }
    pthread_mutex_unlock(&thp -> available_thread_mutex);
    goto poll;
  }

  // case 2: a thread was available and returned by poll_threads, we assign to it a task and pass to it its args 
  // in their corresponding fields
  uint16_t tid = (uint16_t) poll_threads_out;

  // we acquire the mutex to write into the cond and broadcast that a new taks for thread[tid] is available
  pthread_mutex_lock(thp -> available_task_mutex + tid);
  thp -> tasks[tid] = task;
  thp -> args[tid] = args;
  pthread_cond_broadcast(thp -> available_task_cond + tid);
  pthread_mutex_unlock(thp -> available_task_mutex + tid); 
}

void ThreadPool_shutdown(ThreadPool *thp) {

  for (unsigned i = 0; i < thp -> thread_n; i++){
    pthread_mutex_lock(thp -> available_task_mutex + i);
    thp -> exit_status[i] = true;
    pthread_cond_broadcast(thp -> available_task_cond + i);
    pthread_mutex_unlock(thp -> available_task_mutex + i);
  }
  for (unsigned i = 0; i < thp->thread_n; i++) {
    pthread_join(thp->threads[i], NULL);
  }

  free(thp->threads);
  free(thp->tasks);
  free(thp->args);
  free(thp->busy_status);
  free(thp->exit_status);
  thp -> thread_n = 0;
  pthread_mutex_destroy(&thp -> available_thread_mutex);
  pthread_cond_destroy(&thp -> available_thread_cond);
  free(thp->available_task_mutex);
  free(thp->available_task_cond);

}
