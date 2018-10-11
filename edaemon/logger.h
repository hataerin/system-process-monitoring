
#ifndef _INC_LOGGER
#define _INC_LOGGER


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <syscall.h>
#include <sys/stat.h>
#include <stdarg.h>


#ifndef WIN32
#define _getpid() getpid()
#endif

#define		LOG_DEBUG	1
#define		LOG_INFO		2
#define		LOG_ERROR	3

void getdatetime(char* buffer);
void writelog_old(char* buffer);
void writelog(int loglevel, const char* buffer, ...);
int getfilesize(char* filename);

#endif
