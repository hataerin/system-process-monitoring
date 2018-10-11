
#include "logger.h"

extern int gloglevel;

void writelog(int loglevel, const char* buffer, ...)
{
	if (gloglevel > loglevel)
		return;

	char level[8];
	if (loglevel == LOG_DEBUG)
		strcpy(level, "DEBUG");
	else if (loglevel == LOG_INFO)
		strcpy(level, "INFO");
	else if (loglevel == LOG_ERROR)
		strcpy(level, "ERROR");

	char logfilename[128] = { 0, };
	strcpy(logfilename, "edaemon.log");
	char datetime[128] = { 0, };
	FILE* fp = NULL;
	fp = fopen(logfilename, "a");
	getdatetime(datetime);
	fprintf(fp, "%s%s\t", datetime, level);

	va_list	marker;
	va_start(marker, buffer);
	vfprintf(fp, buffer, marker);
	va_end(marker);

	fprintf(fp, "\n");

	fflush(fp);
	fclose(fp);

}


int getfilesize(char* filename)
{
	struct stat fs;
	int ret;
	ret = stat(filename, &fs);
	if (ret != 0)
	{
		return -1;
	}
	return fs.st_size;
}

void getdatetime(char* buffer)
{
	char timeString[128] = { 0, };
	time_t ltime;
	struct tm *today;
	time(&ltime);
	today = localtime(&ltime);
	strftime(timeString, 128, "%Y-%m-%d %H:%M:%S", today);

	struct timeb   now_tb;
	char milliString[16] = { 0, };
	ftime(&now_tb);
	sprintf(milliString, "(%03d)", now_tb.millitm);
	sprintf(buffer, "%s%s(%d)(%d)\t", timeString, milliString, _getpid(), syscall(__NR_gettid));

}
