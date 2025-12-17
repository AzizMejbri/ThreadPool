#include "../../utils/queue/task_queue.h"
#include "../test.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Test counters (if not provided by test.h)
static unsigned tests_passed = 0;
static unsigned tests_failed = 0;

// Helper functions for creating test tasks
static void *dummy_task1(void *arg) {
  static int result = 42;
  (void)arg; // Mark unused parameter
  return &result;
}

static void *dummy_task2(void *arg) {
  char *str = (char *)arg;
  static char buffer[256];
  snprintf(buffer, sizeof(buffer), "Processed: %s", str ? str : "NULL");
  return buffer;
}

static void *null_task(void *arg) {
  (void)arg; // Mark unused parameter
  return NULL;
}

static void *increment_task(void *arg) {
  static int counter = 0;
  int *val = (int *)arg;
  if (val)
    counter += *val;
  return &counter;
}

static void *string_task(void *arg) {
  static char result[256];
  const char *input = (const char *)arg;
  if (input) {
    snprintf(result, sizeof(result), "String: %s", input);
  } else {
    snprintf(result, sizeof(result), "String: NULL");
  }
  return result;
}

#define CREATE_ENTRY(task_func, arg_val)                                       \
  ((TQEntry){task_func, (void *)(arg_val)})

void test_initialization() {
  printf("\n\033[34m=== Test 1: Initialization ===\n\033[0m");

  // Test 1.1: Default initialization (capacity 64)
  TaskQueue *tq1 = TaskQueue_init(0);
  test(tq1 != NULL);
  test(TaskQueue_isempty(tq1) == true);
  test(TaskQueue_size(tq1) == 0);
  TaskQueue_destroy(tq1);

  // Test 1.2: Custom power-of-two capacity
  TaskQueue *tq2 = TaskQueue_init(32);
  test(tq2 != NULL);
  test(TaskQueue_isempty(tq2) == true);
  test(TaskQueue_size(tq2) == 0);
  TaskQueue_destroy(tq2);

  // Test 1.3: Another power-of-two capacity
  TaskQueue *tq3 = TaskQueue_init(128);
  test(tq3 != NULL);
  test(TaskQueue_isempty(tq3) == true);
  test(TaskQueue_size(tq3) == 0);
  TaskQueue_destroy(tq3);
  summary();
}

void test_basic_operations() {
  printf("\n\033[34m=== Test 2: Basic Operations ===\n\033[0m");

  TaskQueue *tq = TaskQueue_init(0);

  // Test 2.1: Single enqueue/dequeue
  TQEntry entry1 = CREATE_ENTRY(dummy_task1, (void *)100);
  TaskQueue_enqueue(tq, entry1);
  test(TaskQueue_isempty(tq) == false);
  test(TaskQueue_size(tq) == 1);

  // Test 2.2: Peek returns the full TQEntry
  TQEntry peeked = TaskQueue_peek(tq);
  test(peeked.task == entry1.task);
  test(peeked.arg == entry1.arg);
  test(TQENTRY_EQ(peeked, entry1));

  // Test 2.3: Dequeue returns the full TQEntry
  TQEntry dequeued = TaskQueue_dequeue(tq);
  test(dequeued.task == entry1.task);
  test(dequeued.arg == entry1.arg);
  test(TQENTRY_EQ(dequeued, entry1));
  test(TaskQueue_isempty(tq) == true);
  test(TaskQueue_size(tq) == 0);

  // Test 2.4: Multiple TQEntry entries
  TQEntry entries[3] = {CREATE_ENTRY(dummy_task1, (void *)1),
                        CREATE_ENTRY(dummy_task2, (void *)2),
                        CREATE_ENTRY(null_task, (void *)3)};

  for (int i = 0; i < 3; i++) {
    TaskQueue_enqueue(tq, entries[i]);
    test(TaskQueue_size(tq) == (i + 1));
  }

  // Verify FIFO order with full TQEntry comparison
  for (int i = 0; i < 3; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, entries[i]));
  }

  test(TaskQueue_isempty(tq) == true);
  TaskQueue_destroy(tq);
  summary();
}

void test_null_entries() {
  printf("\n\033[34m=== Test 3: NULL Entries ===\n\033[0m");

  TaskQueue *tq = TaskQueue_init(0);

  // Test 3.1: Enqueue NULL_ENTRY
  TaskQueue_enqueue(tq, NULL_ENTRY);
  test(TaskQueue_size(tq) == 1);

  TQEntry peeked = TaskQueue_peek(tq);
  test(peeked.task == NULL);
  test(peeked.arg == NULL);
  test(TQENTRY_EQ(peeked, NULL_ENTRY));

  TQEntry dequeued = TaskQueue_dequeue(tq);
  test(dequeued.task == NULL);
  test(dequeued.arg == NULL);
  test(TQENTRY_EQ(dequeued, NULL_ENTRY));
  test(TaskQueue_isempty(tq) == true);

  // Test 3.2: Mix of NULL and non-NULL TQEntry structs
  TQEntry mixed_entries[] = {CREATE_ENTRY(dummy_task1, (void *)1), NULL_ENTRY,
                             CREATE_ENTRY(NULL, (void *)3),
                             CREATE_ENTRY(dummy_task2, NULL),
                             CREATE_ENTRY(NULL, NULL)};

  for (int i = 0; i < 5; i++) {
    TaskQueue_enqueue(tq, mixed_entries[i]);
  }

  for (int i = 0; i < 5; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, mixed_entries[i]));
  }

  TaskQueue_destroy(tq);
  summary();
}

void test_queue_resizing() {
  printf("\n\033[34m=== Test 4: Queue Resizing ===\n\033[0m");

  // Test 4.1: Resize from small capacity
  TaskQueue *tq =
      TaskQueue_init(8); // Will resize when size reaches 6 (8 * 0.75)

  // Store entries to verify later
  TQEntry stored_entries[20];

  for (int i = 0; i < 20; i++) {
    stored_entries[i] = CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)i);
    TaskQueue_enqueue(tq, stored_entries[i]);
  }

  test(TaskQueue_size(tq) == 20);

  // Verify all TQEntry structs are still in order
  for (int i = 0; i < 20; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, stored_entries[i]));
  }

  test(TaskQueue_isempty(tq) == true);
  TaskQueue_destroy(tq);

  // Test 4.2: Multiple resizes
  TaskQueue *tq2 = TaskQueue_init(4);

  // Fill through multiple resize points
  TQEntry many_entries[100];
  for (int i = 0; i < 100; i++) {
    many_entries[i] = CREATE_ENTRY(dummy_task2, (void *)(uintptr_t)i);
    TaskQueue_enqueue(tq2, many_entries[i]);
  }

  test(TaskQueue_size(tq2) == 100);

  // Verify order maintained through resizes
  for (int i = 0; i < 100; i++) {
    TQEntry e = TaskQueue_dequeue(tq2);
    test(TQENTRY_EQ(e, many_entries[i]));
  }

  TaskQueue_destroy(tq2);
  summary();
}

void test_circular_buffer_debug() {
  printf("\n\033[34m=== Test 4.9: Circular Buffer Debug ===\n\033[0m");

  TaskQueue *tq = TaskQueue_init(8);
  printf("Initial: head=%lu, tail=%lu, size=%lu, capacity=%lu\n", tq->head,
         tq->tail, tq->size, tq->capacity);

  printf("\n1. Adding 4 elements (0-3):\n");
  for (int i = 0; i < 4; i++) {
    TaskQueue_enqueue(tq, CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)i));
    printf("  Added %d: head=%lu, tail=%lu, size=%lu\n", i, tq->head, tq->tail,
           tq->size);
  }

  printf("\n2. Removing 2 elements:\n");
  for (int i = 0; i < 2; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    printf("  Removed %lu: head=%lu, tail=%lu, size=%lu\n", (uintptr_t)e.arg,
           tq->head, tq->tail, tq->size);
  }

  printf("\n3. Adding 6 more elements (4-9):\n");
  for (int i = 4; i < 10; i++) {
    TaskQueue_enqueue(tq, CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)i));
    printf("  Added %d: head=%lu, tail=%lu, size=%lu\n", i, tq->head, tq->tail,
           tq->size);
  }

  printf("\n4. Current queue state:\n");
  printf("  head=%lu, tail=%lu, size=%lu, capacity=%lu\n", tq->head, tq->tail,
         tq->size, tq->capacity);

  printf("\n5. Dequeuing all (expecting 2-9):\n");
  for (int i = 2; i < 10; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    uintptr_t val = (uintptr_t)e.arg;
    printf("  Dequeued: %lu (expected: %d) %s\n", val, i,
           val == (uintptr_t)i ? "✓" : "✗");
    if (val != (uintptr_t)i) {
      printf("    ERROR: Mismatch!\n");
    }
  }

  printf("\n6. Final state:\n");
  printf("  head=%lu, tail=%lu, size=%lu, capacity=%lu\n", tq->head, tq->tail,
         tq->size, tq->capacity);
  printf("  isempty: %s\n", TaskQueue_isempty(tq) ? "true" : "false");

  TaskQueue_destroy(tq);
}

void test_circular_buffer_behavior() {
  printf("\n\033[34m=== Test 5: Circular Buffer Behavior ===\n\033[0m");

  TaskQueue *tq = TaskQueue_init(8); // Small capacity to test wrap-around

  // Store entries to verify order
  TQEntry first_entries[4];
  TQEntry later_entries[6];

  // Fill queue halfway
  for (int i = 0; i < 4; i++) {
    first_entries[i] = CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)i);
    TaskQueue_enqueue(tq, first_entries[i]);
  }

  // Remove half to move head
  for (int i = 0; i < 2; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, first_entries[i]));
  }
  // Queue now has [2, 3] at positions 2-3, head=2, tail=4

  // Fill more to cause wrap-around
  for (int i = 4; i < 10; i++) {
    later_entries[i - 4] = CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)i);
    TaskQueue_enqueue(tq, later_entries[i - 4]);
  }
  // Queue should now wrap around: positions 4-7 and 0-1 filled

  test(TaskQueue_size(tq) == 8); // Elements 2-9

  // Expected order: first_entries[2], first_entries[3], then later_entries[0]
  // through later_entries[5]
  for (int i = 2; i < 4; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, first_entries[i]));
  }

  for (int i = 0; i < 6; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, later_entries[i]));
  }

  test(TaskQueue_isempty(tq) == true);

  // Test wrap-around with full cycle
  for (int i = 0; i < 8; i++) {
    TaskQueue_enqueue(tq, CREATE_ENTRY(dummy_task2, (void *)(uintptr_t)i));
  }
  test(TaskQueue_size(tq) == 8);

  for (int i = 0; i < 8; i++) {
    TaskQueue_dequeue(tq);
  }
  test(TaskQueue_isempty(tq) == true);

  TaskQueue_destroy(tq);
  summary();
}

void test_peek_behavior() {
  printf("\n\033[34m=== Test 6: Peek Behavior ===\n\033[0m");

  TaskQueue *tq = TaskQueue_init(0);

  // Test 6.1: Peek on empty queue returns NULL_ENTRY
  TQEntry empty_peek = TaskQueue_peek(tq);
  test(TQENTRY_EQ(empty_peek, NULL_ENTRY));
  test(empty_peek.task == NULL);
  test(empty_peek.arg == NULL);

  // Test 6.2: Peek doesn't modify queue
  TQEntry entry = CREATE_ENTRY(dummy_task1, (void *)42);
  TaskQueue_enqueue(tq, entry);

  TQEntry peek1 = TaskQueue_peek(tq);
  TQEntry peek2 = TaskQueue_peek(tq);
  test(TQENTRY_EQ(peek1, entry));
  test(TQENTRY_EQ(peek2, entry));
  test(TaskQueue_size(tq) == 1); // Size unchanged

  // Test 6.3: Peek after multiple enqueues still shows first TQEntry
  TQEntry entry2 = CREATE_ENTRY(dummy_task2, (void *)43);
  TaskQueue_enqueue(tq, entry2);
  TQEntry peek3 = TaskQueue_peek(tq);
  test(TQENTRY_EQ(peek3, entry)); // Should still be first TQEntry

  // Test 6.4: Peek after dequeue shows next TQEntry
  TQEntry dequeued = TaskQueue_dequeue(tq);
  test(TQENTRY_EQ(dequeued, entry));
  TQEntry peek4 = TaskQueue_peek(tq);
  test(TQENTRY_EQ(peek4, entry2));

  TaskQueue_destroy(tq);
  summary();
}

void test_edge_cases() {
  printf("\n\033[34m=== Test 7: Edge Cases ===\n\033[0m");

  // Test 7.1: Dequeue from empty queue returns NULL_ENTRY
  TaskQueue *tq = TaskQueue_init(0);
  TQEntry e = TaskQueue_dequeue(tq);
  test(TQENTRY_EQ(e, NULL_ENTRY));

  // Test 7.2: Single TQEntry queue
  TQEntry single = CREATE_ENTRY(dummy_task1, (void *)99);
  TaskQueue_enqueue(tq, single);
  test(TaskQueue_size(tq) == 1);
  test(TQENTRY_EQ(TaskQueue_peek(tq), single));

  TQEntry dequeued = TaskQueue_dequeue(tq);
  test(TQENTRY_EQ(dequeued, single));
  test(TaskQueue_isempty(tq) == true);

  // Test 7.3: Fill to exact capacity (power of two)
  TaskQueue *tq2 = TaskQueue_init(16);
  TQEntry capacity_entries[16];

  for (int i = 0; i < 16; i++) {
    capacity_entries[i] = CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)i);
    TaskQueue_enqueue(tq2, capacity_entries[i]);
  }
  test(TaskQueue_size(tq2) == 16);

  // Add one more to trigger resize
  TQEntry extra_entry = CREATE_ENTRY(dummy_task2, (void *)100);
  TaskQueue_enqueue(tq2, extra_entry);
  test(TaskQueue_size(tq2) == 17);

  // Verify all TQEntry structs
  for (int i = 0; i < 16; i++) {
    TQEntry e2 = TaskQueue_dequeue(tq2);
    test(TQENTRY_EQ(e2, capacity_entries[i]));
  }

  TQEntry last = TaskQueue_dequeue(tq2);
  test(TQENTRY_EQ(last, extra_entry));
  test(TaskQueue_isempty(tq2) == true);

  TaskQueue_destroy(tq);
  TaskQueue_destroy(tq2);
  summary();
}

void test_memory_and_destruction() {
  printf("\n\033[34m=== Test 8: Memory and Destruction ===\n\033[0m");

  // Test 8.1: Destroy non-empty queue
  TaskQueue *tq1 = TaskQueue_init(0);
  for (int i = 0; i < 10; i++) {
    TaskQueue_enqueue(tq1, CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)i));
  }
  TaskQueue_destroy(tq1); // Should not leak memory

  // Test 8.2: Multiple queue allocations with TQEntry structs
  TaskQueue *queues[5];
  for (int i = 0; i < 5; i++) {
    queues[i] = TaskQueue_init(1 << (i + 1)); // 2, 4, 8, 16, 32
    for (int j = 0; j < (1 << (i + 1)); j++) {
      // Mix different task types
      if (j % 2 == 0) {
        TaskQueue_enqueue(queues[i],
                          CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)j));
      } else {
        TaskQueue_enqueue(queues[i],
                          CREATE_ENTRY(dummy_task2, (void *)(uintptr_t)j));
      }
    }
  }

  // Clean up
  for (int i = 0; i < 5; i++) {
    TaskQueue_destroy(queues[i]);
  }

  // Test 8.3: Reuse after dequeue all
  TaskQueue *tq2 = TaskQueue_init(0);
  for (int cycle = 0; cycle < 3; cycle++) {
    for (int i = 0; i < 20; i++) {
      TaskQueue_enqueue(tq2, CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)i));
    }
    for (int i = 0; i < 20; i++) {
      TQEntry e = TaskQueue_dequeue(tq2);
      test((uintptr_t)e.arg == (uintptr_t)i);
    }
    test(TaskQueue_isempty(tq2) == true);
  }
  TaskQueue_destroy(tq2);
  summary();
}

void test_concurrent_operations_pattern() {
  printf("\n\033[34m=== Test 9: Concurrent Operations Pattern ===\n\033[0m");

  // Simulate producer-consumer pattern
  TaskQueue *tq = TaskQueue_init(0);
  int total_operations = 1000;

  // Store all TQEntry structs to verify order
  TQEntry all_entries[2000]; // More than we'll use
  int next_value = 0;
  int expected_value = 0;

  for (int i = 0; i < total_operations; i++) {
    // Enqueue between 1-3 items
    int enqueue_count = (i % 3) + 1;
    for (int j = 0; j < enqueue_count; j++) {
      all_entries[next_value] =
          CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)next_value);
      TaskQueue_enqueue(tq, all_entries[next_value]);
      next_value++;
    }

    // Dequeue between 1-2 items
    int dequeue_count = (i % 2) + 1;
    for (int j = 0; j < dequeue_count && !TaskQueue_isempty(tq); j++) {
      TQEntry e = TaskQueue_dequeue(tq);
      // Verify against stored TQEntry
      test(TQENTRY_EQ(e, all_entries[expected_value]));
      expected_value++;
    }
  }

  // Drain remaining queue
  while (!TaskQueue_isempty(tq)) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, all_entries[expected_value]));
    expected_value++;
  }

  test(expected_value == next_value);
  TaskQueue_destroy(tq);
  summary();
}

void test_randomized_stress_test() {
  printf("\n\033[34m=== Test 10: Randomized Stress Test ===\n\033[0m");

  srand(time(NULL));
  TaskQueue *tq = TaskQueue_init(0);

  int operations = 5000;
  int enqueued = 0;
  int dequeued = 0;

  // Store all TQEntry structs
  TQEntry stored_entries[10000]; // More than we need

  for (int i = 0; i < operations; i++) {
    // Randomly choose operation, but ensure we don't dequeue from empty
    if (TaskQueue_isempty(tq) || (rand() % 3) != 0) {
      // Enqueue
      stored_entries[enqueued] =
          CREATE_ENTRY(dummy_task1, (void *)(uintptr_t)enqueued);
      TaskQueue_enqueue(tq, stored_entries[enqueued]);
      enqueued++;
    } else {
      // Dequeue
      TQEntry e = TaskQueue_dequeue(tq);
      test(TQENTRY_EQ(e, stored_entries[dequeued]));
      dequeued++;
    }

    // Verify size is correct
    test(TaskQueue_size(tq) == (enqueued - dequeued));
  }

  // Verify final state
  test(TaskQueue_size(tq) == (enqueued - dequeued));

  // Drain queue and verify all remaining items
  while (!TaskQueue_isempty(tq)) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, stored_entries[dequeued]));
    dequeued++;
  }

  test(enqueued == dequeued);
  test(TaskQueue_isempty(tq) == true);

  TaskQueue_destroy(tq);
  summary();
}

void test_tqentry_eq_macro() {
  printf("\n\033[34m=== Test 11: TQENTRY_EQ Macro ===\n\033[0m");

  // Test equality with TQEntry structs
  TQEntry e1 = {dummy_task1, (void *)1};
  TQEntry e2 = {dummy_task1, (void *)1};
  TQEntry e3 = {dummy_task2, (void *)1};
  TQEntry e4 = {dummy_task1, (void *)2};
  TQEntry e5 = {NULL, NULL};
  TQEntry e6 = {dummy_task1, NULL};
  TQEntry e7 = {NULL, (void *)1};

  test(TQENTRY_EQ(e1, e1) == true);
  test(TQENTRY_EQ(e1, e2) == true);
  test(TQENTRY_EQ(e1, e3) == false);
  test(TQENTRY_EQ(e1, e4) == false);
  test(TQENTRY_EQ(e5, NULL_ENTRY) == true);
  test(TQENTRY_EQ(NULL_ENTRY, NULL_ENTRY) == true);
  test(TQENTRY_EQ(e6, e6) == true);
  test(TQENTRY_EQ(e7, e7) == true);
  test(TQENTRY_EQ(e6, e7) == false);
  test(TQENTRY_EQ(e1, e6) == false);
  test(TQENTRY_EQ(e1, e7) == false);

  summary();
}

void test_function_argument_preservation() {
  printf("\n\033[34m=== Test 12: Function and Argument Preservation ===\n\033[0m");

  TaskQueue *tq = TaskQueue_init(0);

  // Test with various argument types
  int int_arg = 42;
  char *str_arg = "test string";
  float float_arg = 3.14f;

  TQEntry entries[] = {
      CREATE_ENTRY(dummy_task1, &int_arg), CREATE_ENTRY(dummy_task2, str_arg),
      CREATE_ENTRY(increment_task, &int_arg),
      CREATE_ENTRY(string_task, str_arg), CREATE_ENTRY(null_task, &float_arg)};

  // Enqueue all
  for (int i = 0; i < 5; i++) {
    TaskQueue_enqueue(tq, entries[i]);
  }

  // Dequeue and execute to verify arguments are preserved
  for (int i = 0; i < 5; i++) {
    TQEntry e = TaskQueue_dequeue(tq);
    test(TQENTRY_EQ(e, entries[i]));

    // Actually execute the task to verify arguments work
    if (e.task) {
      void *result = e.task(e.arg);
      (void)result; // Use result to avoid unused warning
    }
  }

  test(TaskQueue_isempty(tq) == true);
  TaskQueue_destroy(tq);
  summary();
}

void test_integration_scenario() {
  printf("\n\033[34m=== Test 13: Integration Scenario ===\n\033[0m");

  // Simulate a real-world scenario: processing a batch of tasks
  TaskQueue *tq = TaskQueue_init(0);

  // Phase 1: Load initial TQEntry structs
  const char *urls[] = {"http://example.com", "http://google.com",
                        "http://github.com"};
  TQEntry url_tasks[3];

  for (int i = 0; i < 3; i++) {
    url_tasks[i] = CREATE_ENTRY(dummy_task2, (void *)urls[i]);
    TaskQueue_enqueue(tq, url_tasks[i]);
  }

  // Process one task
  TQEntry processed = TaskQueue_dequeue(tq);
  test(TQENTRY_EQ(processed, url_tasks[0]));
  void *result = processed.task(processed.arg);
  test(result != NULL);

  // Phase 2: Add more TQEntry structs while processing
  TQEntry extra1 = CREATE_ENTRY(dummy_task1, (void *)42);
  TQEntry extra2 = CREATE_ENTRY(null_task, NULL);

  TaskQueue_enqueue(tq, extra1);
  TaskQueue_enqueue(tq, extra2);

  // Process remaining tasks
  int processed_count = 1;
  while (!TaskQueue_isempty(tq)) {
    TQEntry e = TaskQueue_dequeue(tq);
    // Verify we get the expected TQEntry
    if (processed_count == 1)
      test(TQENTRY_EQ(e, url_tasks[1]));
    if (processed_count == 2)
      test(TQENTRY_EQ(e, url_tasks[2]));
    if (processed_count == 3)
      test(TQENTRY_EQ(e, extra1));
    if (processed_count == 4)
      test(TQENTRY_EQ(e, extra2));

    if (e.task) {
      e.task(e.arg); // Execute task
    }
    processed_count++;
  }

  test(processed_count == 5);
  test(TaskQueue_isempty(tq) == true);

  TaskQueue_destroy(tq);
  summary();
}

void test_debug_function() {
  printf("\n\033[34m=== Test 14: Debug Function ===\n\033[0m");

  TaskQueue *tq = TaskQueue_init(4);

  TQEntry entries[] = {CREATE_ENTRY(dummy_task1, (void *)1),
                       CREATE_ENTRY(dummy_task2, (void *)2),
                       CREATE_ENTRY(null_task, (void *)3),
                       CREATE_ENTRY(string_task, "test")};

  for (int i = 0; i < 4; i++) {
    TaskQueue_enqueue(tq, entries[i]);
  }

  printf("Queue state after filling 4 entries:\n");
  TaskQueue_dbg(tq); // Visual inspection

  // Dequeue two and add one more
  TaskQueue_dequeue(tq);
  TaskQueue_dequeue(tq);
  TaskQueue_enqueue(tq, CREATE_ENTRY(increment_task, (void *)5));

  printf("\nQueue state after 2 dequeues and 1 enqueue:\n");
  TaskQueue_dbg(tq); // Visual inspection

  TaskQueue_destroy(tq);
  printf("Debug function test completed\n");
  summary();
}

int main() {
  printf("Starting TaskQueue Test Suite\n");
  printf("=============================\n\n");

  // Initialize test framework - estimate about 250 assertions

  // Run all test suites
  test_initialization();
  test_basic_operations();
  test_null_entries();
  test_queue_resizing();
  test_circular_buffer_behavior();
  test_peek_behavior();
  test_edge_cases();
  test_memory_and_destruction();
  test_concurrent_operations_pattern();
  test_randomized_stress_test();
  test_tqentry_eq_macro();
  test_function_argument_preservation();
  test_integration_scenario();
  test_debug_function();

  printf("\n\033[32m✓ All TaskQueue tests completed!\033[0m\n");

  return 0;
}
