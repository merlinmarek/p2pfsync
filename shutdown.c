#include <stdio.h>
#include <pthread.h>
#include "shutdown.h"

// the shutdown variable has its own lock which MUST BE INITIALIZED ON STARTUP
static pthread_mutex_t shutdownLock;

void initialize_shutdown_lock() {
	if(pthread_mutex_init(&shutdownLock, NULL) != 0) {
		printf("pthread_mutex_init failed\n");
	}
}
void destroy_shutdown_lock() {
	if(pthread_mutex_destroy(&shutdownLock) != 0) {
		printf("pthread_mutex_destroy failed\n");
	}
}

// the shutdown variable is only accessible through these getters/setters
static int shouldShutdown;

void set_shutdown(int shutdown) {
	pthread_mutex_lock(&shutdownLock);
	shouldShutdown = shutdown;
	pthread_mutex_unlock(&shutdownLock);
}

int get_shutdown() {
	int temp = -1;
	pthread_mutex_lock(&shutdownLock);
	temp = shouldShutdown;
	pthread_mutex_unlock(&shutdownLock);
	return temp;
}
