#ifndef NAVI_STDIO_H
#define NAVI_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct navi_file { size_t pos; int used; int error; int eof; int console; } FILE;
extern FILE *stdin,*stdout,*stderr;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define FILENAME_MAX 1024
FILE *fopen(const char *,const char *);
int fclose(FILE *);
size_t fread(void *,size_t,size_t,FILE *);
size_t fwrite(const void *,size_t,size_t,FILE *);
int fseek(FILE *,long,int);
long ftell(FILE *);
int feof(FILE *);
int ferror(FILE *);
void clearerr(FILE *);
void rewind(FILE *);
int fflush(FILE *);
int fileno(FILE *);
int fgetc(FILE *);
int getc(FILE *);
char *fgets(char *,int,FILE *);
int fputc(int,FILE *);
int putc(int,FILE *);
int fputs(const char *,FILE *);
int puts(const char *);
int putchar(int);
int printf(const char *,...);
int fprintf(FILE *,const char *,...);
int sprintf(char *,const char *,...);
int snprintf(char *,size_t,const char *,...);
int vsnprintf(char *,size_t,const char *,va_list);
int vsprintf(char *,const char *,va_list);
int vfprintf(FILE *,const char *,va_list);
int vprintf(const char *,va_list);
int sscanf(const char *,const char *,...);
int fscanf(FILE *,const char *,...);
int remove(const char *);
int rename(const char *,const char *);
void perror(const char *);
#endif
