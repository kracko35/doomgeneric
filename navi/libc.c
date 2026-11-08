/* Small freestanding C runtime for the diskless NHDT emulator port.
 * Only the supplied WAD in guest RAM is exposed as a read-only file.
 * No firmware, host filesystem, network, or OS services are called.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

extern unsigned int navi_wad_size;
extern void navi_exit(int) __attribute__((noreturn));
extern uint32_t DG_GetTicksMs(void);
extern void DG_SleepMs(uint32_t);

int errno;
volatile char navi_log[16384];
volatile unsigned int navi_log_length;
static unsigned char *heap_next=(unsigned char *)0x8d000000u;
#define HEAP_END ((uintptr_t)0x8f000000u)
#define WAD ((const unsigned char *)0x8b000000u)

void *memcpy(void *d,const void *s,size_t n) { unsigned char *a=d; const unsigned char *b=s; while(n--)*a++=*b++; return d; }
void *memset(void *d,int c,size_t n) { unsigned char *a=d; while(n--)*a++=(unsigned char)c; return d; }
void *memmove(void *d,const void *s,size_t n) { unsigned char *a=d; const unsigned char *b=s; if((uintptr_t)a<(uintptr_t)b)return memcpy(d,s,n); while(n) { --n; a[n]=b[n]; } return d; }
int memcmp(const void *a,const void *b,size_t n) { const unsigned char *x=a,*y=b; while(n--) { if(*x!=*y)return *x-*y; ++x;++y; } return 0; }
size_t strlen(const char *s) { const char *p=s; while(*p)++p;return (size_t)(p-s); }
size_t strnlen(const char *s,size_t n) { size_t i=0;while(i<n&&s[i])++i;return i; }
char *strcpy(char *d,const char *s) { char *p=d;while((*p++=*s++)){}return d; }
char *strncpy(char *d,const char *s,size_t n) { char *p=d;while(n&&*s){*p++=*s++;--n;}while(n--)*p++=0;return d; }
char *strcat(char *d,const char *s) { strcpy(d+strlen(d),s);return d; }
char *strncat(char *d,const char *s,size_t n) { char *p=d+strlen(d);while(n--&&*s)*p++=*s++;*p=0;return d; }
int strcmp(const char *a,const char *b) { while(*a&&*a==*b){++a;++b;}return (unsigned char)*a-(unsigned char)*b; }
int strncmp(const char *a,const char *b,size_t n) { while(n&&*a&&*a==*b){++a;++b;--n;}return n?(unsigned char)*a-(unsigned char)*b:0; }
char *strchr(const char *s,int c) { do {if(*s==(char)c)return (char *)s;}while(*s++);return NULL; }
char *strrchr(const char *s,int c) { const char *r=NULL;do {if(*s==(char)c)r=s;}while(*s++);return (char *)r; }
char *strstr(const char *s,const char *needle) { size_t n=strlen(needle);for(;*s;++s)if(!strncmp(s,needle,n))return (char *)s;return n?NULL:(char *)s; }
char *strdup(const char *s) { size_t n=strlen(s)+1;char *r=malloc(n);if(r)memcpy(r,s,n);return r; }
char *strerror(int e) { if(e==ENOENT)return "file not found";if(e==EROFS)return "read-only runtime";if(e==ENOMEM)return "out of guest memory";return "runtime error"; }
size_t strlcpy(char *d,const char *s,size_t n) { size_t len=strlen(s);if(n){size_t k=len<n-1?len:n-1;memcpy(d,s,k);d[k]=0;}return len; }
size_t strlcat(char *d,const char *s,size_t n) { size_t len=strnlen(d,n);if(len==n)return n+strlen(s);return len+strlcpy(d+len,s,n-len); }
int isspace(int c) { return c==' '||(c>='\t'&&c<='\r'); }
int isdigit(int c) { return c>='0'&&c<='9'; }
int isxdigit(int c) { return isdigit(c)||(c>='a'&&c<='f')||(c>='A'&&c<='F'); }
int isalpha(int c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z'); }
int isalnum(int c) { return isalpha(c)||isdigit(c); }
int isprint(int c) { return c>=32&&c<=126; }
int isupper(int c) { return c>='A'&&c<='Z'; }
int islower(int c) { return c>='a'&&c<='z'; }
int toupper(int c) { return islower(c)?c-32:c; }
int tolower(int c) { return isupper(c)?c+32:c; }
int strcasecmp(const char *a,const char *b) { while(*a&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){++a;++b;}return tolower((unsigned char)*a)-tolower((unsigned char)*b); }
int strncasecmp(const char *a,const char *b,size_t n) { while(n&&*a&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){++a;++b;--n;}return n?tolower((unsigned char)*a)-tolower((unsigned char)*b):0; }

/* Doom's zone allocator handles reuse inside its large zone; this outer heap
 * only services startup allocations. free deliberately retains them. */
void *malloc(size_t n) {
    uintptr_t p=((uintptr_t)heap_next+7u)&~(uintptr_t)7u;
    size_t size=n?n:1;
    if(size>HEAP_END-p || HEAP_END-p-size<8u){errno=ENOMEM;return NULL;}
    *(size_t *)p=size;
    heap_next=(unsigned char *)(p+8u+size);
    return (void *)(p+8u);
}
void free(void *p) { (void)p; }
void *calloc(size_t n,size_t size) { if(size&&n>(size_t)-1/size){errno=ENOMEM;return NULL;}size*=n;void *p=malloc(size);if(p)memset(p,0,size);return p; }
void *realloc(void *p,size_t size) { if(!p)return malloc(size);if(!size){free(p);return NULL;}size_t old=*(size_t *)((unsigned char *)p-8);if(size<=old)return p;void *q=malloc(size);if(q)memcpy(q,p,old);return q; }
int abs(int n) { return n<0?-n:n; }
long labs(long n) { return n<0?-n:n; }
static unsigned long parse_unsigned(const char *s,char **end,int base,int *negative) {
    unsigned long r=0;const char *start=s;while(isspace((unsigned char)*s))++s;
    *negative=0;if(*s=='-'||*s=='+'){*negative=*s=='-';++s;}
    if((base==0||base==16)&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')&&isxdigit((unsigned char)s[2])){s+=2;base=16;}
    if(!base)base=*s=='0'?8:10;
    if(base<2||base>36){if(end)*end=(char *)start;errno=EINVAL;return 0;}
    const char *digits=s;
    for(;;){int c=tolower((unsigned char)*s),v=isdigit(c)?c-'0':(c>='a'&&c<='z'?c-'a'+10:99);if(v>=base)break;r=r*(unsigned)base+(unsigned)v;++s;}
    if(end)*end=(char *)(s==digits?start:s);return r;
}
unsigned long strtoul(const char *s,char **end,int base) { int neg;unsigned long r=parse_unsigned(s,end,base,&neg);return neg?0ul-r:r; }
long strtol(const char *s,char **end,int base) { return (long)strtoul(s,end,base); }
int atoi(const char *s) { return (int)strtol(s,NULL,10); }
long atol(const char *s) { return strtol(s,NULL,10); }
double atof(const char *s) { int neg=0;double r=0,scale=1;while(isspace((unsigned char)*s))++s;if(*s=='-'||*s=='+'){neg=*s=='-';++s;}while(isdigit((unsigned char)*s))r=r*10+(*s++-'0');if(*s=='.'){++s;while(isdigit((unsigned char)*s)){scale*=0.1;r+=(*s++-'0')*scale;}}if(*s=='e'||*s=='E'){int e=atoi(s+1);while(e>0&&e<309){r*=10;--e;}while(e<0&&e>-309){r*=0.1;++e;}}return neg?-r:r; }
char *getenv(const char *name) { (void)name;return NULL; }
int system(const char *s) { (void)s;errno=ENOSYS;return -1; }
int atexit(void (*fn)(void)) { (void)fn;return 0; }
void exit(int code) { navi_exit(code); }
void abort(void) { navi_exit(127); }
static unsigned random_state=1;
int rand(void) { random_state=random_state*1103515245u+12345u;return (random_state>>16)&32767; }
void srand(unsigned int seed) { random_state=seed; }
void qsort(void *base,size_t n,size_t size,int (*cmp)(const void *,const void *)) { unsigned char *b=base;for(size_t i=1;i<n;i++)for(size_t j=i;j>0&&cmp(b+(j-1)*size,b+j*size)>0;j--)for(size_t k=0;k<size;k++){unsigned char c=b[(j-1)*size+k];b[(j-1)*size+k]=b[j*size+k];b[j*size+k]=c;} }

static FILE consoles[3]={{0,1,0,0,1},{0,1,0,0,1},{0,1,0,0,1}};
FILE *stdin=&consoles[0],*stdout=&consoles[1],*stderr=&consoles[2];
static FILE files[8];
static int wad_path(const char *path) { const char *b=strrchr(path,'/');return !strcasecmp(b?b+1:path,"doom1.wad"); }
FILE *fopen(const char *path,const char *mode) {
    if(!mode||mode[0]!='r'||strchr(mode,'+')){errno=EROFS;return NULL;}
    if(!path||!wad_path(path)||!navi_wad_size){errno=ENOENT;return NULL;}
    for(unsigned i=0;i<8;i++)if(!files[i].used){memset(&files[i],0,sizeof(FILE));files[i].used=1;return &files[i];}
    errno=EMFILE;return NULL;
}
int fclose(FILE *f) { if(!f||!f->used){errno=EBADF;return EOF;}if(!f->console)f->used=0;return 0; }
size_t fread(void *dest,size_t size,size_t count,FILE *f) {
    if(!f||!f->used||f->console){errno=EBADF;return 0;}
    if(!size||!count)return 0;
    if(count>(size_t)-1/size){errno=EOVERFLOW;f->error=1;return 0;}
    size_t want=size*count,avail=f->pos<navi_wad_size?navi_wad_size-f->pos:0,bytes=want<avail?want:avail;
    memcpy(dest,WAD+f->pos,bytes);f->pos+=bytes;if(bytes<want)f->eof=1;return bytes/size;
}
static void log_char(char c) { unsigned n=navi_log_length;if(n<sizeof(navi_log)-1){navi_log[n]=c;navi_log[n+1]=0;navi_log_length=n+1;} }
size_t fwrite(const void *src,size_t size,size_t count,FILE *f) { if(f&&f->console){const char *s=src;if(size&&count>(size_t)-1/size)return 0;for(size_t i=0;i<size*count;i++)log_char(s[i]);return count;}errno=EROFS;if(f)f->error=1;return 0; }
int fseek(FILE *f,long off,int whence) { if(!f||!f->used||f->console){errno=EBADF;return -1;}long base=whence==SEEK_SET?0:whence==SEEK_CUR?(long)f->pos:whence==SEEK_END?(long)navi_wad_size:-1;if(base<0||off< -base || off>(long)navi_wad_size-base){errno=EINVAL;return -1;}f->pos=(size_t)(base+off);f->eof=0;return 0; }
long ftell(FILE *f) { if(!f||!f->used){errno=EBADF;return -1;}return (long)f->pos; }
int feof(FILE *f) { return f?f->eof:1; }
int ferror(FILE *f) { return f?f->error:1; }
void clearerr(FILE *f) { if(f)f->error=f->eof=0; }
void rewind(FILE *f) { (void)fseek(f,0,SEEK_SET);clearerr(f); }
int fflush(FILE *f) { (void)f;return 0; }
int fileno(FILE *f) { if(f==stdin)return 0;if(f==stdout)return 1;if(f==stderr)return 2;for(int i=0;i<8;i++)if(f==files+i&&f->used)return i+3;errno=EBADF;return -1; }
int fgetc(FILE *f) { unsigned char c;return fread(&c,1,1,f)==1?c:EOF; }
int getc(FILE *f) { return fgetc(f); }
char *fgets(char *s,int n,FILE *f) { if(n<1)return NULL;int i=0,c;while(i<n-1&&(c=fgetc(f))!=EOF){s[i++]=(char)c;if(c=='\n')break;}s[i]=0;return i?s:NULL; }
int fputc(int c,FILE *f) { unsigned char b=(unsigned char)c;return fwrite(&b,1,1,f)==1?b:EOF; }
int putc(int c,FILE *f) { return fputc(c,f); }
int fputs(const char *s,FILE *f) { size_t n=strlen(s);return fwrite(s,1,n,f)==n?0:EOF; }
int puts(const char *s) { fputs(s,stdout);return putchar('\n'); }
int putchar(int c) { return fputc(c,stdout); }

typedef struct { char *buffer;size_t capacity,length; } output;
static void emit(output *o,char c) { if(o->capacity&&o->length<o->capacity-1)o->buffer[o->length]=c;++o->length; }
static void field(output *o,const char *s,size_t len,int width,int left,char pad) { int p=width>(int)len?width-(int)len:0;if(!left)while(p--)emit(o,pad);for(size_t i=0;i<len;i++)emit(o,s[i]);if(left)while(p--)emit(o,' '); }
int vsnprintf(char *dest,size_t capacity,const char *fmt,va_list args) {
    output out={dest,capacity,0};
    while(*fmt){if(*fmt!='%'){emit(&out,*fmt++);continue;}++fmt;
        int left=0,zero=0,plus=0,alt=0,width=0,precision=-1,longs=0;
        for(;;){if(*fmt=='-')left=1;else if(*fmt=='0')zero=1;else if(*fmt=='+')plus=1;else if(*fmt=='#')alt=1;else if(*fmt!=' ')break;++fmt;}
        if(*fmt=='*'){width=va_arg(args,int);++fmt;if(width<0){left=1;width=-width;}}else while(isdigit((unsigned char)*fmt))width=width*10+(*fmt++-'0');
        if(*fmt=='.'){++fmt;precision=0;if(*fmt=='*'){precision=va_arg(args,int);++fmt;}else while(isdigit((unsigned char)*fmt))precision=precision*10+(*fmt++-'0');}
        if(*fmt=='l'){++longs;++fmt;if(*fmt=='l'){++longs;++fmt;}}else if(*fmt=='z'){longs=sizeof(size_t)==sizeof(long)?1:0;++fmt;}else if(*fmt=='h'){++fmt;if(*fmt=='h')++fmt;}
        char spec=*fmt?*fmt++:0;
        if(spec=='%'){emit(&out,'%');continue;}
        if(spec=='s'){const char *s=va_arg(args,const char *);if(!s)s="(null)";size_t n=precision>=0?strnlen(s,(size_t)precision):strlen(s);field(&out,s,n,width,left,' ');continue;}
        if(spec=='c'){char c=(char)va_arg(args,int);field(&out,&c,1,width,left,' ');continue;}
        if(spec=='n'){*va_arg(args,int *)=(int)out.length;continue;}
        char tmp[96],rev[32];int n=0,digits=0,negative=0;unsigned long long value=0;unsigned base=10;
        if(spec=='f'||spec=='F'){double v=va_arg(args,double);if(v<0){tmp[n++]='-';v=-v;}int prec=precision<0?6:precision;if(prec>9)prec=9;unsigned long whole=(unsigned long)v;double frac=v-whole;do{rev[digits++]=(char)('0'+whole%10);whole/=10;}while(whole);while(digits)tmp[n++]=rev[--digits];if(prec){tmp[n++]='.';while(prec--){frac*=10;int d=(int)frac;tmp[n++]=(char)('0'+d);frac-=d;}}field(&out,tmp,n,width,left,' ');continue;}
        if(spec=='d'||spec=='i'){long long v=longs==2?va_arg(args,long long):longs==1?va_arg(args,long):va_arg(args,int);negative=v<0;value=negative?0ull-(unsigned long long)v:(unsigned long long)v;}
        else if(spec=='u'||spec=='x'||spec=='X'||spec=='o'){value=longs==2?va_arg(args,unsigned long long):longs==1?va_arg(args,unsigned long):va_arg(args,unsigned int);if(spec=='x'||spec=='X')base=16;else if(spec=='o')base=8;}
        else if(spec=='p'){value=(uintptr_t)va_arg(args,void *);base=16;alt=1;}
        else {emit(&out,'%');if(spec)emit(&out,spec);continue;}
        do {unsigned d=(unsigned)(value%base);rev[digits++]=(char)(d<10?'0'+d:(spec=='X'?'A':'a')+d-10);value/=base;}while(value&&digits<(int)sizeof(rev));
        if(negative)tmp[n++]='-';else if(plus&&(spec=='d'||spec=='i'))tmp[n++]='+';
        if(alt&&base==16){tmp[n++]='0';tmp[n++]=spec=='X'?'X':'x';}else if(alt&&base==8&&rev[digits-1]!='0')tmp[n++]='0';
        int prefix=n,pad=precision>digits?precision-digits:0;
        if(zero&&!left&&precision<0&&width>n+digits)pad=width-n-digits;
        if(pad>(int)sizeof(tmp)-n-digits)pad=(int)sizeof(tmp)-n-digits;
        while(pad-->0)tmp[n++]='0';while(digits)tmp[n++]=rev[--digits];
        (void)prefix;field(&out,tmp,(size_t)n,width,left,' ');
    }
    if(capacity)dest[out.length<capacity?out.length:capacity-1]=0;
    return (int)out.length;
}
int snprintf(char *d,size_t n,const char *f,...) { va_list a;va_start(a,f);int r=vsnprintf(d,n,f,a);va_end(a);return r; }
int vsprintf(char *d,const char *f,va_list a) { return vsnprintf(d,(size_t)-1,f,a); }
int sprintf(char *d,const char *f,...) { va_list a;va_start(a,f);int r=vsprintf(d,f,a);va_end(a);return r; }
int vfprintf(FILE *stream,const char *fmt,va_list args) { char b[1024];int n=vsnprintf(b,sizeof(b),fmt,args);size_t out=n<(int)sizeof(b)?(size_t)n:sizeof(b)-1;fwrite(b,1,out,stream);return n; }
int fprintf(FILE *s,const char *f,...) { va_list a;va_start(a,f);int r=vfprintf(s,f,a);va_end(a);return r; }
int vprintf(const char *f,va_list a) { return vfprintf(stdout,f,a); }
int printf(const char *f,...) { va_list a;va_start(a,f);int r=vprintf(f,a);va_end(a);return r; }

static int scan(const char *s,const char *fmt,va_list args) { int count=0;while(*fmt){if(isspace((unsigned char)*fmt)){while(isspace((unsigned char)*s))++s;++fmt;continue;}if(*fmt!='%'){if(*fmt++!=*s++)break;continue;}++fmt;int width=0;while(isdigit((unsigned char)*fmt))width=width*10+(*fmt++-'0');char spec=*fmt++;if(spec=='%'){if(*s++!='%')break;continue;}while(isspace((unsigned char)*s))++s;if(!*s)break;if(spec=='d'||spec=='i'||spec=='x'||spec=='o'||spec=='u'){char *end;int base=spec=='x'?16:spec=='o'?8:spec=='i'?0:10;unsigned long value=strtoul(s,&end,base);if(end==s)break;*va_arg(args,int *)=(int)value;s=end;++count;}else if(spec=='s'){char *d=va_arg(args,char *);if(!width)width=0x7fffffff;while(*s&&!isspace((unsigned char)*s)&&width--)*d++=*s++;*d=0;++count;}else if(spec=='['){char *d=va_arg(args,char *);int invert=*fmt=='^';if(invert)++fmt;const char *set=fmt;while(*fmt&&*fmt!=']')++fmt;size_t len=fmt-set;if(*fmt==']')++fmt;if(!width)width=0x7fffffff;int copied=0;while(*s&&width--){int member=0;for(size_t i=0;i<len;i++)if(set[i]==*s)member=1;if(member==invert)break;*d++=*s++;copied=1;}*d=0;if(!copied)break;++count;}else break;}return count; }
int sscanf(const char *s,const char *f,...) { va_list a;va_start(a,f);int r=scan(s,f,a);va_end(a);return r; }
int fscanf(FILE *stream,const char *fmt,...) { char line[256];if(!fgets(line,sizeof(line),stream))return EOF;va_list a;va_start(a,fmt);int r=scan(line,fmt,a);va_end(a);return r; }
int remove(const char *p) { (void)p;errno=EROFS;return -1; }
int rename(const char *a,const char *b) { (void)a;(void)b;errno=EROFS;return -1; }
void perror(const char *s) { fprintf(stderr,"%s: %s\n",s,strerror(errno)); }
int mkdir(const char *s,mode_t m) { (void)s;(void)m;errno=EROFS;return -1; }
int stat(const char *s,struct stat *st) { if(!wad_path(s)||!navi_wad_size){errno=ENOENT;return -1;}st->st_mode=S_IFREG|0444;st->st_size=navi_wad_size;return 0; }
int access(const char *s,int mode) { if(mode&W_OK){errno=EROFS;return -1;}struct stat st;return stat(s,&st); }
int isatty(int fd) { (void)fd;return 0; }
static FILE *fd_file(int fd) { if(fd>=0&&fd<3)return consoles+fd;if(fd>=3&&fd<11&&files[fd-3].used)return files+fd-3;errno=EBADF;return NULL; }
int open(const char *path,int flags,...) { if(flags!=O_RDONLY){errno=EROFS;return -1;}FILE *f=fopen(path,"rb");return f?fileno(f):-1; }
int close(int fd) { FILE *f=fd_file(fd);return f?fclose(f):-1; }
ssize_t read(int fd,void *b,size_t n) { FILE *f=fd_file(fd);return f?(ssize_t)fread(b,1,n,f):-1; }
ssize_t write(int fd,const void *b,size_t n) { FILE *f=fd_file(fd);return f?(ssize_t)fwrite(b,1,n,f):-1; }
off_t lseek(int fd,off_t o,int w) { FILE *f=fd_file(fd);return f&&fseek(f,o,w)==0?ftell(f):-1; }
int fstat(int fd,struct stat *st) { FILE *f=fd_file(fd);if(!f)return -1;st->st_mode=S_IFREG|0444;st->st_size=f->console?0:navi_wad_size;return 0; }
unsigned int sleep(unsigned int seconds) { DG_SleepMs(seconds*1000u);return 0; }
int usleep(unsigned int micros) { DG_SleepMs((micros+999u)/1000u);return 0; }
int unlink(const char *path) { return remove(path); }
char *getcwd(char *b,size_t n) { if(n<2){errno=EINVAL;return NULL;}b[0]='.';b[1]=0;return b; }
time_t time(time_t *out) { time_t t=DG_GetTicksMs()/1000u;if(out)*out=t;return t; }
clock_t clock(void) { return DG_GetTicksMs(); }
int gettimeofday(struct timeval *tv,void *zone) { unsigned t=DG_GetTicksMs();(void)zone;tv->tv_sec=t/1000u;tv->tv_usec=(t%1000u)*1000u;return 0; }

/* The Ubuntu SH compiler ships a little-endian libgcc only. These ordinary
 * C ABI helpers are built with the same -mb settings as the engine, avoiding
 * any dependence on that archive. No division instruction/helper is used. */
static uint64_t divide64(uint64_t numerator,uint64_t denominator,uint64_t *remainder) {
    uint64_t result=0,rest=0;
    if(!denominator) { if(remainder)*remainder=numerator;return ~(uint64_t)0; }
    for(unsigned i=0;i<64;i++) {
        unsigned overflow=(unsigned)(rest>>63);
        rest=(rest<<1)|(numerator>>63);
        numerator<<=1;
        result<<=1;
        if(overflow||rest>=denominator){rest-=denominator;result|=1;}
    }
    if(remainder)*remainder=rest;
    return result;
}
uint64_t __udivdi3(uint64_t n,uint64_t d) { return divide64(n,d,NULL); }
uint64_t __umoddi3(uint64_t n,uint64_t d) { uint64_t r;divide64(n,d,&r);return r; }
uint64_t __udivmoddi4(uint64_t n,uint64_t d,uint64_t *r) { return divide64(n,d,r); }
int64_t __divdi3(int64_t n,int64_t d) { uint64_t a=n<0?0ull-(uint64_t)n:(uint64_t)n,b=d<0?0ull-(uint64_t)d:(uint64_t)d;uint64_t q=divide64(a,b,NULL);return (int64_t)(((n<0)!=(d<0))?0ull-q:q); }
int64_t __moddi3(int64_t n,int64_t d) { uint64_t a=n<0?0ull-(uint64_t)n:(uint64_t)n,b=d<0?0ull-(uint64_t)d:(uint64_t)d,r;divide64(a,b,&r);return (int64_t)(n<0?0ull-r:r); }
static uint32_t divide32(uint32_t numerator,uint32_t denominator,uint32_t *remainder) {
    uint32_t result=0,rest=0;
    if(!denominator){if(remainder)*remainder=numerator;return ~0u;}
    for(unsigned i=0;i<32;i++) {
        unsigned overflow=rest>>31;
        rest=(rest<<1)|(numerator>>31);numerator<<=1;result<<=1;
        if(overflow||rest>=denominator){rest-=denominator;result|=1;}
    }
    if(remainder)*remainder=rest;return result;
}
uint32_t __udivsi3(uint32_t n,uint32_t d) { return divide32(n,d,NULL); }
uint32_t __umodsi3(uint32_t n,uint32_t d) { uint32_t r;divide32(n,d,&r);return r; }
int32_t __divsi3(int32_t n,int32_t d) { uint32_t a=n<0?0u-(uint32_t)n:(uint32_t)n,b=d<0?0u-(uint32_t)d:(uint32_t)d,q=divide32(a,b,NULL);return (int32_t)(((n<0)!=(d<0))?0u-q:q); }
int32_t __modsi3(int32_t n,int32_t d) { uint32_t a=n<0?0u-(uint32_t)n:(uint32_t)n,b=d<0?0u-(uint32_t)d:(uint32_t)d,r;divide32(a,b,&r);return (int32_t)(n<0?0u-r:r); }
