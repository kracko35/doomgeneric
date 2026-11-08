#ifndef NAVI_UNISTD_H
#define NAVI_UNISTD_H
#include <stddef.h>
#include <sys/types.h>
#define F_OK 0
#define R_OK 4
#define W_OK 2
int access(const char *,int);
int isatty(int);
int close(int);
ssize_t read(int,void *,size_t);
ssize_t write(int,const void *,size_t);
off_t lseek(int,off_t,int);
unsigned int sleep(unsigned int);
int usleep(unsigned int);
int unlink(const char *);
char *getcwd(char *,size_t);
#endif
