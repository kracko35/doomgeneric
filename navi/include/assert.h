#ifndef NAVI_ASSERT_H
#define NAVI_ASSERT_H
#include <stdlib.h>
#include <stdio.h>
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((x)?(void)0:(fprintf(stderr,"assert: %s:%d: %s\n",__FILE__,__LINE__,#x),abort()))
#endif
#endif
