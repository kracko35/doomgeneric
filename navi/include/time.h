#ifndef NAVI_TIME_H
#define NAVI_TIME_H
#include <sys/types.h>
typedef unsigned long clock_t;
#define CLOCKS_PER_SEC 1000
time_t time(time_t *);
clock_t clock(void);
#endif
