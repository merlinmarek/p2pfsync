#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include "logger.h"

static pthread_mutex_t logger_lock;
static log_level_t current_log_level = LOG_DEBUG;
static int log_file = -1;
static int log_file_creation_failed = 0; // if we faile to create a log file we do not want to retry so the command line output is not flooded with error messages

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

	// this probably should be somewhere else but this is easiest for now
	if(log_file != -1) {
		close(log_file);
	}
}

void set_log_level(log_level_t level) {
	pthread_mutex_lock(&logger_lock);
	current_log_level = level;
	pthread_mutex_unlock(&logger_lock);
}

void logger(log_level_t level, const char* file, const char* function, const char* format_string, ...) {
	pthread_mutex_lock(&logger_lock);
	if(log_file == -1 && log_file_creation_failed == 0) {
		// we need to open a new log file
		// first generate a file name from the current date
		char date_buffer[128];
		strcpy(date_buffer, "./log/");
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);
        strftime(date_buffer + strlen(date_buffer), sizeof(date_buffer) - strlen(date_buffer), "%d_%m_%Y_%a_%T", &tm);
        log_file = open(date_buffer, O_RDWR|O_CREAT, 0664);
        if(log_file == -1) {
        	printf("The log file could not be created: %s\nNo logs will be written.\n", strerror(errno));
        	log_file_creation_failed = 1; // so we do not retry...
        }
	}
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

        if(log_file != -1) {
            if(level > LOG_ERROR) {
                dprintf(log_file, "[%-14s %-7s %-20s %-25s] ", timestamp_buffer, "INVALID", basename(file_path_buffer), function);
            } else {
                dprintf(log_file, "[%-14s %-7s %-20s %-25s] ", timestamp_buffer, log_level_strings[level], basename(file_path_buffer), function);
            }
            vdprintf(log_file, format_string, arguments);
            fsync(log_file); // flush so we do not lose any data if the application crashes
            va_end(arguments);
            va_start(arguments, format_string);
        }

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
