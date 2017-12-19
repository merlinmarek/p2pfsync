#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "broadcast.h"
#include "command.h"
#include "fileTransfer.h"
#include "logger.h"
#include "peerList.h"
#include "shutdown.h"
#include "util.h"

void sigintHandler(int unused) {
	LOGD("\nreceived SIGINT, shutting down...\n");
	setShutdown(1);
}

// the main function
// - sets up all locks
// - sets up the threads
// - cleans up when all thread are down
int main() {
	// first initialize all the locks
	initializeShutdownLock();
	initializeLoggerLock();
	initializePeerListLock();

	setShutdown(0); // make sure we do not shutdown right after starting

	pthread_t broadcastThreadId;
	pthread_t commandThreadId;
	pthread_t fileTransferThreadId;

	int success;

	success = pthread_create(&broadcastThreadId, NULL, broadcastThread, (void*)0);
	if(success != 0) {
		LOGE("pthread_create failed with return code %d\n", success);
	}
	success = pthread_create(&commandThreadId, NULL, commandThread, (void*)0);
	if(success != 0) {
		LOGE("pthread_create failed with return code %d\n", success);
	}
	success = pthread_create(&fileTransferThreadId, NULL, fileTransferThread, (void*)0);
	if(success != 0) {
		LOGE("pthread_create failed with return code %d\n", success);
	}

	// setup sigint handler
	signal(SIGINT, sigintHandler);

	while(!getShutdown()) {
		sleep(1);
	}

	// join all threads
	pthread_join(broadcastThreadId, NULL);
	pthread_join(commandThreadId, NULL);
	pthread_join(fileTransferThreadId, NULL);

	LOGD("threads are down\n");

	// cleanup
	freePeerList();

	// destroy all locks
	destroyShutdownLock();
	destroyPeerListLock();
	destroyLoggerLock();

	pthread_exit(NULL); // should be at end of main function
}
