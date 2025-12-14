#include <stdint.h>
#include <sched.h>

#include "../test.h"
#include "../../utils/cores.h"

int main(){
  init_test(1);
  test(logical_cores_count() == 4);
  summary();
  return 0;
}
