#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H


/**
 * @brief Thread Safety
 * @note This implementation is NOT thread-safe. For concurrent access,
 *       external synchronization must be used.
 * 
 * @brief Memory Management
 * @note The queue dynamically resizes when 75% full. Memory is allocated
 *       in powers of two for efficient modulo operations.
 * 
 * @brief Error Handling
 * @warning Memory allocation failures are not fully handled. In production
 *          environments, consider adding error return codes.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef void *(*Task)(void *);
typedef void *Arg;

typedef struct {
  Task task;
  Arg arg;
} TQEntry;

#define NULL_ENTRY ((TQEntry){NULL, NULL})

#define TQENTRY_EQ(entry1, entry2)                                             \
  (entry1.task == entry2.task && entry1.arg == entry2.arg)

typedef struct {
  uint64_t head;
  uint64_t tail;
  uint64_t size;
  uint64_t capacity;
  TQEntry *entries;
} TaskQueue;

// NOTE:
// default capacity 64 so that it allows a minor optimization:
// head <- (head + 1) % capacity => head <- (head + 1) & (capacity - 1)
// make sure init_cap = 2^x where x \in IN

/**
 * @brief Initialize a new task queue with specified capacity
 *
 * @param init_cap Initial capacity of the queue (must be power of 2).
 *                 If 0 is provided, defaults to 64.
 * @return TaskQueue* Pointer to the newly created task queue, or NULL on
 * failure
 *
 * @note The implementation uses a circular buffer with power-of-two capacity
 *       to enable efficient modulo operations using bitwise AND.
 *       Initial capacity defaults to 64 (2^6) to allow the optimization:
 *       head = (head + 1) & (capacity - 1) instead of head = (head + 1) %
 * capacity
 *
 * @warning Always ensure init_cap is a power of two for correct operation
 * @warning The caller is responsible for calling TaskQueue_destroy() to free
 * resources
 */
TaskQueue *TaskQueue_init(uint64_t init_cap);

/**
 * @brief Check if the task queue is empty
 *
 * @param tq Pointer to the task queue
 * @return bool true if queue is empty, false otherwise
 *
 * @note This function provides O(1) constant time check
 * @warning tq must not be NULL
 */
bool TaskQueue_isempty(TaskQueue *tq);

/**
 * @brief Get the current number of tasks in the queue
 *
 * @param tq Pointer to the task queue
 * @return uint64_t Number of tasks currently in the queue
 *
 * @note This function provides O(1) constant time access
 * @warning tq must not be NULL
 */
uint64_t TaskQueue_size(TaskQueue *tq);

/**
 * @brief Peek at the front task without removing it
 *
 * @param tq Pointer to the task queue
 * @return Task The task at the front of the queue,
 *          or NULL if queue is empty
 *
 * @note This function provides O(1) constant time access
 * @note Always check if queue is empty using TaskQueue_isempty() before calling
 *       to avoid unnecessary NULL checks
 * @warning tq must not be NULL
 */
TQEntry TaskQueue_peek(TaskQueue *tq);

/**
 * @brief Add a task to the end of the queue
 *
 * @param tq Pointer to the task queue
 * @param t Task to enqueue and its arguments
 * @return returns true if the enqueing was successful and false otherwise
 *
 * @note Automatically resizes the queue when it reaches 75% capacity
 * @note The resize doubles the current capacity
 * @note This function provides amortized O(1) time complexity
 * @warning tq must not be NULL
 */
bool TaskQueue_enqueue(TaskQueue *tq, TQEntry t);

/**
 * @brief Remove and return the task from the front of the queue
 *
 * @param tq Pointer to the task queue
 * @return Task The task removed from the front,
 *          or NULL if queue is empty
 *
 * @note This function provides O(1) constant time access
 * @note Always check if queue is empty using TaskQueue_isempty() before calling
 *       to avoid unnecessary NULL checks
 * @warning tq must not be NULL
 */
TQEntry TaskQueue_dequeue(TaskQueue *tq);

/**
 * @brief Destroy the task queue and free all allocated memory
 *
 * @param tq Pointer to the task queue to destroy
 *
 * @note This function frees both the task array and the queue structure itself
 * @warning After calling this function, the queue pointer becomes invalid
 * @warning This function does not free individual tasks stored in the queue
 * @warning tq must not be NULL
 */
void TaskQueue_destroy(TaskQueue *tq);

void TaskQueue_dbg(TaskQueue *tq);

#endif
