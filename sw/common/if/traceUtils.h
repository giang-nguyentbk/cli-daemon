/*
* ______________________   ________                                     
* __  ____/__  /____  _/   ___  __ \_____ ____________ ________________ 
* _  /    __  /  __  /     __  / / /  __ `/  _ \_  __ `__ \  __ \_  __ \
* / /___  _  /____/ /      _  /_/ // /_/ //  __/  / / / / / /_/ /  / / /
* \____/  /_____/___/      /_____/ \__,_/ \___//_/ /_/ /_/\____//_/ /_/ 
*                                                                       
*/

#ifndef __TRACE_UTILS_H__
#define __TRACE_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#define __USE_GNU
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/prctl.h>
#include <sched.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

void LOG_INFO_ZZ(const char *file, int line, const char *format, ...);
void LOG_ERROR_ZZ(const char *file, int line, const char *format, ...);
void LOG_ABN_ZZ(const char *file, int line, const char *format, ...);
void LOG_DEBUG_ZZ(const char *file, int line, const char *format, ...);

#define LOG_INFO(format, ...) LOG_INFO_ZZ(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) LOG_ERROR_ZZ(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ABN(format, ...) LOG_ABN_ZZ(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) LOG_DEBUG_ZZ(__FILENAME__, __LINE__, format, ##__VA_ARGS__)

/*
	By high-resolution time measurement, it's showed that using LOG_MACRO() only costs 200% overhead compared to original printf().
	Using printf() consumes about 35-40 us while using LOG_MACRO(), it's 65-75 us. It's still in acceptable ranges, right? (-_-)

	By the way, using "inline" keyword for these LOG_MACRO_ZZ apparently has no effect! That's interesting as well!
*/

void LOG_INFO_ZZ(const char *file, int line, const char *format, ...)
{
	va_list args;
	char buffer[256];

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	char timestamp[50];
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);
	struct tm *timePointerEnd = localtime(&tv.tv_sec);
	size_t nbytes = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timePointerEnd);
	snprintf(timestamp + nbytes, sizeof(timestamp) - nbytes, ".%.9ld", tv.tv_nsec);

	char threadname[30];
	threadname[0] = '\"';
	prctl(PR_GET_NAME, &threadname[1], (30 - 2));
	int len = strlen(threadname);
	threadname[len] = '\"';
	threadname[len + 1] = '\0';

	char fileline[64];
	snprintf(fileline, 64, "%s:%d", file, line);

	char cpuid_buffer[5];
	unsigned int cpuid = 999;
	getcpu(&cpuid, NULL);
	if(cpuid != 999)
	{
		sprintf(cpuid_buffer, "%d", cpuid);
	} else
	{
		cpuid_buffer[0] = '-';
		cpuid_buffer[1] = '\0';
	}

	fprintf(stdout, "%-30s %-10s cpu=%-5s %-20s %-25s msg: %-s", timestamp, "INFO", cpuid_buffer, threadname, fileline, buffer);
	fflush(stdout);
}

void LOG_ERROR_ZZ(const char *file, int line, const char *format, ...)
{
	va_list args;
	char buffer[256];

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	char timestamp[50];
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);
	struct tm *timePointerEnd = localtime(&tv.tv_sec);
	size_t nbytes = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timePointerEnd);
	snprintf(timestamp + nbytes, sizeof(timestamp) - nbytes, ".%.9ld", tv.tv_nsec);

	char threadname[30];
	threadname[0] = '\"';
	prctl(PR_GET_NAME, &threadname[1], (30 - 2));
	int len = strlen(threadname);
	threadname[len] = '\"';
	threadname[len + 1] = '\0';

	char fileline[64];
	snprintf(fileline, 64, "%s:%d", file, line);

	char cpuid_buffer[5];
	unsigned int cpuid = 999;
	getcpu(&cpuid, NULL);
	if(cpuid != 999)
	{
		sprintf(cpuid_buffer, "%d", cpuid);
	} else
	{
		cpuid_buffer[0] = '-';
		cpuid_buffer[1] = '\0';
	}

	fprintf(stdout, "%-30s %-10s cpu=%-5s %-20s %-25s msg: %-s", timestamp, "ERROR", cpuid_buffer, threadname, fileline, buffer);
	fflush(stdout);
}

void LOG_ABN_ZZ(const char *file, int line, const char *format, ...)
{
	va_list args;
	char buffer[256];

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	char timestamp[50];
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);
	struct tm *timePointerEnd = localtime(&tv.tv_sec);
	size_t nbytes = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timePointerEnd);
	snprintf(timestamp + nbytes, sizeof(timestamp) - nbytes, ".%.9ld", tv.tv_nsec);

	char threadname[30];
	threadname[0] = '\"';
	prctl(PR_GET_NAME, &threadname[1], (30 - 2));
	int len = strlen(threadname);
	threadname[len] = '\"';
	threadname[len + 1] = '\0';

	char fileline[64];
	snprintf(fileline, 64, "%s:%d", file, line);

	char cpuid_buffer[5];
	unsigned int cpuid = 999;
	getcpu(&cpuid, NULL);
	if(cpuid != 999)
	{
		sprintf(cpuid_buffer, "%d", cpuid);
	} else
	{
		cpuid_buffer[0] = '-';
		cpuid_buffer[1] = '\0';
	}

	fprintf(stdout, "%-30s %-10s cpu=%-5s %-20s %-25s msg: %-s", timestamp, "ABN", cpuid_buffer, threadname, fileline, buffer);
	fflush(stdout);
}

void LOG_DEBUG_ZZ(const char *file, int line, const char *format, ...)
{
	va_list args;
	char buffer[256];

	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	char timestamp[30];
	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);
	struct tm *timePointerEnd = localtime(&tv.tv_sec);
	size_t nbytes = strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timePointerEnd);
	snprintf(timestamp + nbytes, sizeof(timestamp) - nbytes, ".%.9ld", tv.tv_nsec);

	char threadname[30];
	threadname[0] = '\"';
	prctl(PR_GET_NAME, &threadname[1], (30 - 2));
	int len = strlen(threadname);
	threadname[len] = '\"';
	threadname[len + 1] = '\0';

	char fileline[64];
	snprintf(fileline, 64, "%s:%d", file, line);

	char cpuid_buffer[5];
	unsigned int cpuid = 999;
	getcpu(&cpuid, NULL);
	if(cpuid != 999)
	{
		sprintf(cpuid_buffer, "%d", cpuid);
	} else
	{
		cpuid_buffer[0] = '-';
		cpuid_buffer[1] = '\0';
	}

	fprintf(stdout, "%-30s %-10s cpu=%-5s %-20s %-25s msg: %-s", timestamp, "DEBUG", cpuid_buffer, threadname, fileline, buffer);
	fflush(stdout);
}



#ifdef __cplusplus
}
#endif

#endif // __TRACE_UTILS_H__