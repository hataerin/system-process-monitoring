#ifndef _INC_DB
#define _INC_DB


#include <stdio.h>
#include <string>
#include <sys/types.h>
#include <assert.h>
#include <memory.h>

#ifndef WIN32
#include <unistd.h>
#endif


int initialize();
int connect(const char* cname, const char* uname, const char* passwd);


int commit();
int rollback();
int disconnect();
int uninitialize();
int execute_insert(char* msg);

#endif
