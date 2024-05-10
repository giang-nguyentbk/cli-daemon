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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

void LOG_INFO_ZZ(const char *file, int line, const char *format, ...);
void LOG_ERROR_ZZ(const char *file, int line, const char *format, ...);
void LOG_ABN_ZZ(const char *file, int line, const char *format, ...);
void LOG_DEBUG_ZZ(const char *file, int line, const char *format, ...);

#define LOG_INFO(format, ...) LOG_INFO_ZZ(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) LOG_ERROR_ZZ(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ABN(format, ...) LOG_ABN_ZZ(__FILENAME__, __LINE__, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) LOG_DEBUG_ZZ(__FILENAME__, __LINE__, format, ##__VA_ARGS__)

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
	snprintf(timestamp + nbytes, sizeof(timestamp) - nbytes,
		"%.9ld", tv.tv_nsec);

	char fileline[40];
	snprintf(fileline, 40, "%s:%d", file, line);
	fprintf(stdout, "%-25s %-10s %-40s msg: %-s\n", timestamp, "INFO:", fileline, buffer);
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
	snprintf(timestamp + nbytes, sizeof(timestamp) - nbytes,
		"%.9ld", tv.tv_nsec);

	char fileline[40];
	snprintf(fileline, 40, "%s:%d", file, line);
	fprintf(stdout, "%-25s %-10s %-40s msg: %-s\n", timestamp, "ERROR:", fileline, buffer);
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
	snprintf(timestamp + nbytes, sizeof(timestamp) - nbytes,
		"%.9ld", tv.tv_nsec);

	char fileline[40];
	snprintf(fileline, 40, "%s:%d", file, line);
	fprintf(stdout, "%-25s %-10s %-40s msg: %-s\n", timestamp, "ABN:", fileline, buffer);
	fflush(stdout);
}

void LOG_DEBUG_ZZ(const char *file, int line, const char *format, ...)
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
	snprintf(timestamp + nbytes, sizeof(timestamp) - nbytes,
		"%.9ld", tv.tv_nsec);

	char fileline[40];
	snprintf(fileline, 40, "%s:%d", file, line);
	fprintf(stdout, "%-25s %-10s %-40s msg: %-s\n", timestamp, "DEBUG:", fileline, buffer);
	fflush(stdout);
}



#ifdef __cplusplus
}
#endif

#endif // __TRACE_UTILS_H__