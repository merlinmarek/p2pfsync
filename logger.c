#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include "logger.h"

static pthread_mutex_t loggerLock;

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

void logger(const char* formatString, ...) {
	pthread_mutex_lock(&loggerLock);
	va_list arguments;
	va_start(arguments, formatString);
	vprintf(formatString, arguments);
	va_end(arguments);
	pthread_mutex_unlock(&loggerLock);
};
