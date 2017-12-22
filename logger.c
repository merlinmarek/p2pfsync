#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

#include "logger.h"

static pthread_mutex_t logger_lock;
static log_level_t current_log_level = LOG_DEBUG;

static const char* log_level_strings[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
};

void initialize_logger_lock() {
	if(pthread_mutex_init(&logger_lock, NULL) != 0) {
		printf("pthread_mutex_init failed\n");
	}
}
void destroy_logger_lock() {
	if(pthread_mutex_destroy(&logger_lock) != 0) {
		printf("pthread_mutex_destroy failed\n");
	}
}

void set_log_level(log_level_t level) {
	pthread_mutex_lock(&logger_lock);
	current_log_level = level;
	pthread_mutex_unlock(&logger_lock);
}

void logger(log_level_t level, const char* file, const char* function, const char* format_string, ...) {
	pthread_mutex_lock(&logger_lock);
	if(level >= current_log_level) {
        va_list arguments;
        va_start(arguments, format_string);

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
        	printf("[%-14s %-7s %-20s %-25s] ", timestamp_buffer, "INVALID", basename(file_path_buffer), function);
        } else {
        	printf("[%-14s %-7s %-20s %-25s] ", timestamp_buffer, log_level_strings[level], basename(file_path_buffer), function);
        }
        vprintf(format_string, arguments);
        va_end(arguments);
	}
	pthread_mutex_unlock(&logger_lock);
};
