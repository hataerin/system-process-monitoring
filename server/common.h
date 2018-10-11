#ifndef _INC_COMMON_H
#define _INC_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/timeb.h>

#ifdef WIN32
#include <process.h>
#else
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <syscall.h>
#endif
#include <sys/stat.h>
#include <stdarg.h>

#endif


#ifndef WIN32
#define _getpid() getpid()
#endif
