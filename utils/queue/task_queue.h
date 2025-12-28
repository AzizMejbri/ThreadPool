
#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

/**
 * @file task_queue.h
 * @brief Lock-free multi-producer multi-consumer task queue
 *
 * @thread_safety
 * - Fully lock-free
 * - Supports multiple producers and multiple consumers (MPMC)
 * - Uses C11 atomics
 * - No mutexes or blocking synchronization
 *
 * @memory_model
 * - Enqueue uses release semantics
 * - Dequeue uses acquire semantics
 * - Linearizable enqueue/dequeue
 *
 * @warning
 * - TaskQueue_peek() is NOT linearizable with dequeue
 * - Peek must only be used for debugging or heuristics
 */

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "../../lib/arena/components/arena.h"

typedef void *(*Task)(void *);
typedef void *Arg;

typedef struct {
  Task task;
  Arg arg;
} TQEntry;

#define NULL_ENTRY ((TQEntry){NULL, NULL})
#define TQENTRY_EQ(a, b) ((a).task == (b).task && (a).arg == (b).arg)

/* Internal node (Michael–Scott queue) */
typedef struct TQNode {
  TQEntry entry;
  _Atomic(struct TQNode *) next;
} TQNode;

/**
 * @brief Lock-free task queue
 */
typedef struct {
  Arena arena;
  _Atomic(TQNode *) head;
  _Atomic(TQNode *) tail;
} TaskQueue;

/**
 * @brief Initialize a lock-free task queue
 *
 * @param tq          Queue instance
 * @param arena_size  Arena size in bytes (0 = heap fallback)
 *
 * @return true on success
 */
bool TaskQueue_init(TaskQueue *tq, uint64_t arena_size);

/**
 * @brief Enqueue a task (lock-free, linearizable)
 */
bool TaskQueue_enqueue(TaskQueue *tq, TQEntry entry);

/**
 * @brief Dequeue a task (lock-free, linearizable)
 *
 * @return NULL_ENTRY if queue is empty
 */
TQEntry TaskQueue_dequeue(TaskQueue *tq);

/**
 * @brief Peek front element (NOT linearizable)
 */
TQEntry TaskQueue_peek(TaskQueue *tq);

/**
 * @brief Destroy queue and release arena
 *
 * @warning Must be called after all threads stop using the queue
 */
void TaskQueue_destroy(TaskQueue *tq);

/**
 * @brief Debug print (non-thread-safe)
 */
void TaskQueue_dbg(TaskQueue *tq);

#endif
