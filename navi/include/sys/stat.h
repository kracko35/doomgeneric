#ifndef NAVI_SYS_STAT_H
#define NAVI_SYS_STAT_H
#include <sys/types.h>
struct stat { mode_t st_mode; off_t st_size; };
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_ISREG(m) (((m)&0170000)==S_IFREG)
#define S_ISDIR(m) (((m)&0170000)==S_IFDIR)
int mkdir(const char *,mode_t);
int stat(const char *,struct stat *);
int fstat(int,struct stat *);
#endif
