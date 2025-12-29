
# ThreadPool - High-Performance C Thread Pool Library

## Overview

ThreadPool is a lightweight, high-performance thread pool implementation in C designed for modern multi-core systems. It provides two distinct threading models - **Static** (fixed-size) and **Cached** (dynamic scaling) - built on top of a lock-free SPMC (Single Producer Multiple Consumer) queue for maximum throughput.

## Features

- **Dual Threading Models**: Choose between fixed-size static pools or dynamically scaling cached pools
- **Lock-Free Task Queue**: Custom SPMC queue minimizes contention and synchronization overhead
- **Adaptive Scaling**: Cached pool automatically adjusts thread count based on workload
- **Memory Efficient**: Thread retirement in cached mode prevents resource waste
- **Thread-Safe API**: Full atomic operations for thread safety
- **High Throughput**: Optimized for short-lived tasks with minimal overhead

## Performance Highlights

Tested on Intel Core i7 vPro (2 physical cores, 4 logical cores), Arch Linux x64, 8GB RAM:

### Static ThreadPool
- **Throughput**: 450K - 690K tasks/second
- **Average**: ~500K tasks/second
- **Best Case**: 690,771 tasks/second

### Cached ThreadPool
- **Throughput**: ~480K tasks/second consistently
- **Adaptive**: Automatically scales from 0 to N threads
- **Resource Efficient**: Idle threads timeout after 1 second

## Architecture

### ThreadPool Models

#### 1. Static ThreadPool
- Fixed thread count specified at initialization
- Threads remain alive for pool lifetime
- Ideal for predictable workloads with consistent parallelism requirements

#### 2. Cached ThreadPool
- Dynamically creates threads on demand
- Idle threads timeout and terminate after configurable period
- Perfect for bursty or unpredictable workloads
- Maximum thread count scales to available cores

### Underlying Queue
- **SPMC (Single Producer Multiple Consumer)** lock-free queue
- Main thread submits tasks (producer)
- Worker threads consume tasks (consumers)
- Atomic operations for synchronization
- Memory barriers ensure proper visibility

## API Reference

### Types

```c
typedef void *(*Task)(void *);           // Task function signature
typedef void *Args;                      // Task arguments
typedef enum { THREADPOOL_CACHED, THREADPOOL_STATIC } ThreadPoolType;
```

### Functions
```c
// Initialize thread pool
void ThreadPool_init(ThreadPool *thp, unsigned int thread_num, int flags);

// Submit task for execution
bool ThreadPool_execute(ThreadPool *thp, Task task, Args args);

// Submit task with timeout (TODO)
void ThreadPool_execute_with_timeout(ThreadPool *thp, Task task, 
                                     Args args, unsigned timeout);

// Gracefully shutdown thread pool
void ThreadPool_shutdown(ThreadPool *thp);
```

### Macros

```c
// Check if no pending tasks remain
#define TQ_EMPTY(thp) (atomic_load_explicit(&thp->pending_tasks, memory_order_acquire) == 0)
```


## Installation & Building
### Prerequisites

- C11 compiler (GCC/Clang)

- POSIX threads (pthreads)

- Linux/Unix system (tested on Arch Linux x64)

### Build Instructions
```bash
# Clone and build
git clone https://github.com/AzizMejbri/ThreadPool
cd ThreadPool

make install

# For debug builds
```

### Usage Examples
#### Basic Static ThreadPool

```c
#include "ThreadPool.h"
#include <stdio.h>

void* process_data(void* arg) {
    int* data = (int*)arg;
    printf("Processing: %d\n", *data);
    // Process data...
    return NULL;
}

int main() {
    ThreadPool pool;
    
    // Initialize with 4 threads (static mode)
    ThreadPool_init(&pool, 4, THREADPOOL_STATIC);
    
    // Submit tasks
    int tasks[100];
    for (int i = 0; i < 100; i++) {
        tasks[i] = i;
        ThreadPool_execute(&pool, process_data, &tasks[i]);
    }
    
    // Wait for completion
    while (!TQ_EMPTY(&pool)) {
        // Do other work or sleep
        sched_yield();
    }
    
    // Cleanup
    ThreadPool_shutdown(&pool);
    return 0;
}
```

#### Adaptive Cached ThreadPool

```c
#include "ThreadPool.h"
#include <stdatomic.h>

_Atomic int task_counter = 0;

void* quick_task(void* arg) {
    // Simulate work
    for (volatile int i = 0; i < 1000; i++);
    atomic_fetch_add(&task_counter, 1);
    return NULL;
}

int main() {
    ThreadPool pool;
    
    // Initialize cached pool (0 initial threads)
    ThreadPool_init(&pool, 0, THREADPOOL_CACHED);
    
    // Burst of 1000 tasks
    for (int i = 0; i < 1000; i++) {
        ThreadPool_execute(&pool, quick_task, NULL);
    }
    
    // Pool automatically scales up threads
    
    // Wait for all tasks
    while (atomic_load(&task_counter) != 1000) {
        sched_yield();
    }
    
    // Threads will timeout and terminate after 1 second idle
    sleep(2);
    
    ThreadPool_shutdown(&pool);
    return 0;
}
```

## Performance Considerations
### Throughput Formula
The theoretical throughput follows:

$$ Throughput = \frac {N}{(1 + \alpha(N-1) + \beta(N-1))}\text{  Where:}
\begin{cases}
N: & \text{Number of threads}
\\[4pt]
\alpha: & \text{Contention factor (lock waiting)}
\\[4pt]
\beta: & \text{Coherency factor (cache invalidation)}
\end{cases}
$$

### Recommendations

- Static Pool: Use when task count is predictable and sustained

- Cached Pool: Best for bursty workloads with idle periods

- Task Granularity: Balance between too fine (overhead) and too coarse (underutilization)

- Core Count: For CPU-bound tasks, limit threads to logical cores

- I/O-bound Tasks: Can use more threads than cores

### Benchmark Results
#### Test Environment
- CPU: Intel Core i7 vPro (2P + 4L cores)

- OS: [![Arch Linux x86_64](https://img.shields.io/badge/Arch%20Linux%20x64-1793D1?logo=arch-linux&logoColor=fff)](#)

- Memory: 8GB RAM

- Compiler: GCC with -O3 optimizations

- Typical Results
```text
Static ThreadPool (8 threads):
  Best:    690,771 tasks/sec
  Average: ~500,000 tasks/sec
  Worst:   450,029 tasks/sec

Cached ThreadPool (adaptive):
  Consistent: ~480,000 tasks/sec
  Scaling:    0 → N threads as needed
```
## Advanced Usage
### Monitoring Pool Status

```c
// Check pending tasks
uint64_t pending = atomic_load(&pool.pending_tasks);

// Get current thread count
uint16_t active_threads = atomic_load(&pool.thread_n);

// Check if pool is idle
if (TQ_EMPTY(&pool)) {
    printf("All tasks completed\n");
}
```

### Custom Task Functions

```c
typedef struct {
    int id;
    double* data;
    size_t length;
} ComputationTask;

void* complex_computation(void* arg) {
    ComputationTask* task = (ComputationTask*)arg;
    
    // Process task->data of length task->length
    for (size_t i = 0; i < task->length; i++) {
        task->data[i] = /* computation */;
    }
    
    free(task->data);
    free(task);
    return NULL;
}

// Submit complex task
ComputationTask* task = malloc(sizeof(ComputationTask));
task->data = allocate_data();
task->length = data_size;
ThreadPool_execute(&pool, complex_computation, task);
```

### Best Practices
#### Task Design

- Keep tasks reasonably sized (100μs - 10ms ideal)

- Avoid tasks that block for long periods

- Use thread-local storage when possible

#### Memory Management

- Pass arguments by reference (allocate if needed)

- Free memory in task function or after completion

- Consider memory pools for high-frequency tasks

- Error Handling

- Check ThreadPool_execute return value

- Implement task-level error reporting

- Use atomic flags for task status tracking

#### Shutdown Pattern

- Always call ThreadPool_shutdown before exit

- Ensure all tasks complete before shutdown

- Handle SIGINT for graceful shutdown

## TODO / Roadmap

1. Implement ThreadPool_execute_with_timeout

2. Add thread affinity support

3. Implement work stealing for better load balancing

4. Add priority queue support

5. Performance profiling hooks

6. Dynamic timeout adjustment for cached pool


## License
this C ThreadPool implementation is released under the MIT License. This means you can use, modify, and distribute the software with very few restrictions.

The Only Requirement:
- Include the copyright notice and license text when distributing

For the full license text, see the LICENSE file in the repository.

## Acknowledgments
- Inspired by Java's ThreadPoolExecutor and .NET's ThreadPool

- Performance analysis using Amdahl's Law and Gunther's Universal Scalability Law

## Notes

* For optimal performance, tune the IDLE_TIMEOUT_NS constant based on your workload characteristics and system resources.
* Optimized for modern x64 multi-core Linux systems

