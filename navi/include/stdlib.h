#ifndef NAVI_STDLIB_H
#define NAVI_STDLIB_H
#include <stddef.h>
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767
void *malloc(size_t);
void *calloc(size_t,size_t);
void *realloc(void *,size_t);
void free(void *);
int abs(int);
long labs(long);
int atoi(const char *);
long atol(const char *);
long strtol(const char *,char **,int);
unsigned long strtoul(const char *,char **,int);
double atof(const char *);
char *getenv(const char *);
int system(const char *);
int atexit(void (*)(void));
void exit(int) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
void qsort(void *,size_t,size_t,int (*)(const void *,const void *));
int rand(void);
void srand(unsigned int);
#endif
