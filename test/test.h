#ifndef TEST_H
#define TEST_H

#include <stdio.h>

#define SUCCESS "\033[36m"
#define ALMOSTSUCCESS "\033[32m"
#define SEMISUCCESS "\033[93m"
#define MID "\033[33m"
#define FAILURE "\033[31m"
#define ABSFAILURE "\033[91m"

static unsigned test_num = 0;
static unsigned successes = 0;
static unsigned failures = 0;

#define test(x)                                                                \
  do {                                                                         \
    if (x) {                                                                   \
      printf("\033[32m✓\033[m");                                               \
      successes++;                                                             \
    } else {                                                                   \
      printf("\033[31m✘\033[m");                                               \
      failures++;                                                              \
    }                                                                          \
    test_num++;                                                                \
  } while (0)

#define summary()                                                              \
  do {                                                                         \
    char* col = SUCCESS;                                                       \
    double ratio = (double)successes / (double)test_num;                       \
    if ( ratio == 1 ) col = SUCCESS;                                           \
    else if (.9 <= ratio && ratio < 1)                                         \
      col = ALMOSTSUCCESS;                                                     \
    else if (.7 <= ratio && ratio < .9)                                        \
      col = SEMISUCCESS;                                                       \
    else if (.5 <= ratio && ratio < .7)                                        \
      col = MID;                                                               \
    else if (.3 <= ratio && ratio < .5)                                        \
      col = FAILURE;                                                           \
    else                                                                       \
      col = ABSFAILURE;                                                        \
    printf("\n%sEnd Result: %f%%\033[m", col, ratio * 100);                    \
  } while (0)

#endif
