#include <stdlib.h>
#include <unistd.h>


#include "../test.h"
#include "../../components/ThreadPool/ThreadPool.h"



void* printd(void* x){
  printf(" %d ", *(int*)x);
  fflush(stdout);
  return NULL;
}


int main(){
  ThreadPool thp;
  int arg[10000] = {0};
  for (unsigned i = 0; i < 10000; i++) arg[i] = i;

  ThreadPool_init(&thp, 0, THREADPOOL_STATIC);
  for (unsigned i = 0; i < 1000; i++)
    ThreadPool_execute(&thp, (Task)printd, arg + i);
  // ThreadPool_execute(&thp, (Task)printd, arg + 1);
  // ThreadPool_execute(&thp, (Task)printd, arg + 2);
  // ThreadPool_execute(&thp, (Task)printd, arg + 3);
  // ThreadPool_execute(&thp, (Task)printd, arg + 4);
  // ThreadPool_execute(&thp, (Task)printd, arg + 5);
  sleep(1);
  ThreadPool_shutdown(&thp);
  return 0;
}
