#include <stdlib.h>


#include "../test.h"
#include "../../components/ThreadPool/ThreadPool.h"




void* printd(void* x){
  printf(" %d ", *(int*)x);
  return NULL;
}


int main(){
  ThreadPool* thp = malloc(sizeof(ThreadPool));
  int arg[6] = {1,2,3,4,5,6};
  ThreadPool_init(thp, 0, THREADPOOL_STATIC);
  ThreadPool_execute(thp, printd, arg);
  ThreadPool_execute(thp, printd, arg + 1);
  ThreadPool_execute(thp, printd, arg + 2);
  ThreadPool_execute(thp, printd, arg + 3);
  ThreadPool_execute(thp, printd, arg + 4);
  ThreadPool_execute(thp, printd, arg + 5);
  ThreadPool_shutdown(thp);
  free(thp);
  return 0;
}
