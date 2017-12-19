#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include "logger.h"

static pthread_mutex_t loggerLock;
static LogLevel current_log_level = LOG_DEBUG;

static const char* LogLevelStrings[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
};

void initializeLoggerLock() {
	if(pthread_mutex_init(&loggerLock, NULL) != 0) {
		printf("pthread_mutex_init failed\n");
	}
}
void destroyLoggerLock() {
	if(pthread_mutex_destroy(&loggerLock) != 0) {
		printf("pthread_mutex_destroy failed\n");
	}
}

void set_log_level(LogLevel level) {
	pthread_mutex_lock(&loggerLock);
	current_log_level = level;
	pthread_mutex_unlock(&loggerLock);
}

void logger(LogLevel level, const char* file, const char* function, const char* formatString, ...) {
	pthread_mutex_lock(&loggerLock);
	if(level >= current_log_level) {
        va_list arguments;
        va_start(arguments, formatString);

        // the log level has to be in the range DEBUG ... ERROR and the file name has to be
        // reduced to its basename

        // first copy the file name
        char file_path_buffer[PATH_MAX];
        strncpy(file_path_buffer, file, PATH_MAX);

        // we should also get the current time and print some timestamp
        char timestamp_buffer[128];
        struct timeval now;
        gettimeofday(&now, NULL);
        struct tm* now_local = localtime(&now.tv_sec);
        strftime(timestamp_buffer, sizeof(timestamp_buffer), "%d.%m %T", now_local);

        if(level > LOG_ERROR) {
        	printf("[%-14s %-7s %-20s %-20s] ", timestamp_buffer, "INVALID", basename(file_path_buffer), function);
        } else {
        	printf("[%-14s %-7s %-20s %-20s] ", timestamp_buffer, LogLevelStrings[level], basename(file_path_buffer), function);
        }
        vprintf(formatString, arguments);
        va_end(arguments);
	}
	pthread_mutex_unlock(&loggerLock);
};
