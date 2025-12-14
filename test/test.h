#ifndef TEST_H
#define TEST_H

#include <stdio.h>

#define SUCCESS "\x1b[36m"
#define ALMOSTSUCCESS "\x1b[32m"
#define SEMISUCCESS "\x1b[93m"
#define MID "\x1b[33m"
#define FAILURE "\x1b[31m"
#define ABSFAILURE "\x1b[91m"

static unsigned test_num = 0;
static unsigned successes = 0;
static unsigned failures = 0;

#define init_test(n)                                                           \
  do {                                                                         \
    test_num = n;                                                              \
  } while (0)

#define test(x)                                                                \
  do {                                                                         \
    if (x) {                                                                   \
      printf("\x1b[32m✓\x1b[m");                                               \
      successes++;                                                             \
    } else {                                                                   \
      printf("\x1b[31m✘\x1b[m");                                               \
      failures++;                                                              \
    }                                                                          \
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
    printf("\n%sEnd Result: %f\x1b[m", col, ratio);                            \
  } while (0)

#endif
