#include "task_queue.h"

#include <arena.h>
#include <pretty.h>
#include <stdio.h>

#include "../../lib/arena/components/arena.h"

bool TaskQueue_init(TaskQueue *q, uint64_t arena_size) {
  q->arena = arena_create(arena_size);
  if (!q->arena.head)
    return false;

  TQNode *dummy = arena_alloc_zeroed(&q->arena, sizeof(TQNode));
  if (!dummy)
    return false;

  dummy->entry = NULL_ENTRY;
  atomic_store_explicit(&dummy->next, NULL, memory_order_relaxed);

  atomic_store_explicit(&q->head, dummy, memory_order_relaxed);
  atomic_store_explicit(&q->tail, dummy, memory_order_relaxed);

  return true;
}

// bool TaskQueue_enqueue(TaskQueue *q, TQEntry entry) {
//   TQNode *node = arena_alloc(&q->arena, sizeof(TQNode));
//   if (!node)
//     return false;
//   node->entry = entry;
//   atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
//   TQNode *prev_tail =
//       atomic_exchange_explicit(&q->tail, node, memory_order_acq_rel);
//   atomic_store_explicit(&prev_tail->next, node, memory_order_release);
//   return true;
// }

bool TaskQueue_enqueue(TaskQueue *q, TQEntry entry) {
  TQNode *node = arena_alloc(&q->arena, sizeof(TQNode));
  if (!node)
    return false;
  
  node->entry = entry;
  atomic_store_explicit(&node->next, NULL, memory_order_relaxed);
  
  while (1) {
    TQNode *tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    TQNode *next = atomic_load_explicit(&tail->next, memory_order_acquire);
    
    // Check if tail is still the last node
    TQNode *current_tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    if (tail != current_tail)
      continue;  // Tail was modified, retry
    
    if (next == NULL) {
      // Tail is consistent, try to link new node
      if (atomic_compare_exchange_weak_explicit(&tail->next, &next, node,
                                                 memory_order_release,
                                                 memory_order_acquire)) {
        // Success! Try to swing tail to new node (best effort)
        atomic_compare_exchange_weak_explicit(&q->tail, &tail, node,
                                               memory_order_release,
                                               memory_order_acquire);
        return true;
      }
    } else {
      // Tail is lagging, help by swinging it forward
      atomic_compare_exchange_weak_explicit(&q->tail, &tail, next,
                                             memory_order_release,
                                             memory_order_acquire);
    }
  }
}

TQEntry TaskQueue_dequeue(TaskQueue *q) {
  while (1) {
    TQNode *head = atomic_load_explicit(&q->head, memory_order_acquire);

    TQNode *next = atomic_load_explicit(&head->next, memory_order_acquire);

    if (next == NULL)
      return NULL_ENTRY; // empty

    if (atomic_compare_exchange_weak_explicit(&q->head, &head, next,
                                              memory_order_acq_rel,
                                              memory_order_acquire)) {
      return next->entry;
    }
  }
}

void TaskQueue_destroy(TaskQueue *q) { arena_destroy(&q->arena); }

TQEntry TaskQueue_peek(TaskQueue *q) {
  TQNode *head = atomic_load_explicit(&q->head, memory_order_acquire);
  TQNode *next = atomic_load_explicit(&head->next, memory_order_acquire);

  if (!next)
    return NULL_ENTRY;

  return next->entry;
}
