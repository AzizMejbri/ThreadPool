#include "task_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <pretty.h>


TaskQueue *TaskQueue_init(uint64_t init_cap) {
  TaskQueue *tq = malloc(sizeof(TaskQueue));
  if ( tq == NULL ){
    error("Couldnt allocate memory for Task Queue, abort now ...");
    return NULL;
  }
  if (init_cap != 0)
    tq->capacity = init_cap;
  else
    tq->capacity = 64;
  tq->size = 0;
  tq->head = 0;
  tq->tail = 0;
  tq->entries = calloc(tq->capacity, sizeof(TQEntry));
  if ( tq -> entries == NULL ){
    error("Couldnt allocate memory for Task Queue Entries, abort now ...");
    free(tq);
    return NULL;
  }
  return tq;
}

bool TaskQueue_isempty(TaskQueue *tq) { return tq->size == 0; }

uint64_t TaskQueue_size(TaskQueue *tq) { return tq->size; }

TQEntry TaskQueue_peek(TaskQueue *tq) {
  if (tq->size == 0) {
    return NULL_ENTRY;
  }
  return tq->entries[tq->head];
}

bool TaskQueue_enqueue(TaskQueue *tq, TQEntry t) {
  if (tq->size * 4 >= tq->capacity * 3) {
    uint64_t old_capacity = tq -> capacity;
    tq->capacity *= 2; 
    TQEntry* tmp_entries = malloc(tq -> capacity * sizeof(TQEntry));
    if (tmp_entries == NULL){
      return false;
    }
    uint64_t old_idx = 0;
    for(unsigned i = 0; i < tq -> size; i++){
      old_idx = (tq -> head + i) & (old_capacity - 1); 
      tmp_entries[i] = tq -> entries[old_idx];
    }
    free(tq -> entries);
    tq -> entries = tmp_entries;
    tq -> head = 0;
    tq -> tail = tq -> size;
  }
  tq->entries[tq->tail] = t;
  tq->tail = (tq->tail + 1) & (tq->capacity - 1);
  tq->size++;
  return true;
}

TQEntry TaskQueue_dequeue(TaskQueue *tq) {
  if (tq->size == 0) {
    return NULL_ENTRY;
  }
  TQEntry ret = tq->entries[tq->head];
  tq->head = (tq->head + 1) & (tq->capacity - 1);
  tq->size--;
  return ret;
}

void TaskQueue_destroy(TaskQueue *tq) {
  free(tq->entries);
  free(tq);
}

void TaskQueue_dbg(TaskQueue *tq) {
  if (tq->size == 0) {
    printf("[]\n");
    return;
  }
  uint64_t beg = tq->head;
  uint64_t end = tq->tail;
  printf("[ ");
  while (beg != end) {
    printf(" (%p, %p) ", tq->entries[beg].task, tq->entries[beg].arg);
    beg = (beg + 1) & (tq->capacity - 1);
  }
  printf("]\n");
}
