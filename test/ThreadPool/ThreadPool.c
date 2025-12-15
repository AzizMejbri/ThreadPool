#include <stdlib.h>


#include "../test.h"
#include "../../components/ThreadPool/ThreadPool.h"



void* printd(void* x){
  printf(" %d ", *(int*)x);
  fflush(stdout);
  return NULL;
}


int main(){
  ThreadPool thp;
  int arg[6] = {0,1,2,3,4,5};
  ThreadPool_init(&thp, 0, THREADPOOL_STATIC);
  ThreadPool_execute(&thp, (Task)printd, arg);
  ThreadPool_execute(&thp, (Task)printd, arg + 1);
  ThreadPool_execute(&thp, (Task)printd, arg + 2);
  ThreadPool_execute(&thp, (Task)printd, arg + 3);
  ThreadPool_execute(&thp, (Task)printd, arg + 4);
  ThreadPool_execute(&thp, (Task)printd, arg + 5);
  ThreadPool_shutdown(&thp);
  return 0;
}
