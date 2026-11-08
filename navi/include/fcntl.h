#ifndef NAVI_FCNTL_H
#define NAVI_FCNTL_H
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 64
#define O_TRUNC 512
int open(const char *,int,...);
#endif
