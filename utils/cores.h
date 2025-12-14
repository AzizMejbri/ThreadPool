#ifndef CORES_H
#define CORES_H

#include <stdint.h>

static inline uint16_t logical_cores_count(){
  cpu_set_t set;
  CPU_ZERO(&set);
  sched_getaffinity(0, sizeof(set), &set);
  return CPU_COUNT(&set);
}



#endif
