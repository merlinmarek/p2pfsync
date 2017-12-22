#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "broadcast.h"
#include "command_client.h"
#include "command_server.h"
#include "file_client.h"
#include "file_server.h"
#include "logger.h"
#include "peer_list.h"
#include "shutdown.h"
#include "util.h"

void sigint_handler(int unused) {
	LOGD("received SIGINT, shutting down...\n");
	set_shutdown(1);
}

// the main function
// - sets up all locks
// - sets up the threads
// - cleans up when all thread are down
int main() {
	// first initialize all the locks
	initialize_shutdown_lock();
	initialize_logger_lock();
	initialize_peer_list_lock();

	set_shutdown(0); // make sure we do not shutdown right after starting

	set_log_level(LOG_DEBUG);

	pthread_t broadcast_thread_id;
	pthread_t command_client_thread_id;
	pthread_t command_server_thread_id;
	pthread_t file_client_thread_id;
	pthread_t file_server_thread_id;

	int success;

	success = pthread_create(&broadcast_thread_id, NULL, broadcast_thread, (void*)0);
	if(success != 0) {
		LOGE("pthread_create failed with return code %d\n", success);
	}
	success = pthread_create(&command_client_thread_id, NULL, command_client_thread, (void*)0);
	if(success != 0) {
		LOGE("pthread_create failed with return code %d\n", success);
	}
	success = pthread_create(&command_server_thread_id, NULL, command_server_thread, (void*)0);
	if(success != 0) {
		LOGE("pthread_create failed with return code %d\n", success);
	}
	success = pthread_create(&file_client_thread_id, NULL, file_client_thread, (void*)0);
	if(success != 0) {
		LOGE("pthread_create failed with return code %d\n", success);
	}
	success = pthread_create(&file_server_thread_id, NULL, file_server_thread, (void*)0);
	if(success != 0) {
		LOGE("pthread_create failed with return code %d\n", success);
	}

	// setup sigint handler
	signal(SIGINT, sigint_handler);

	while(!get_shutdown()) {
		sleep(1);
	}

	// join all threads
	pthread_join(broadcast_thread_id, NULL);
	pthread_join(command_client_thread_id, NULL);
	pthread_join(command_server_thread_id, NULL);
	pthread_join(file_client_thread_id, NULL);
	pthread_join(file_server_thread_id, NULL);

	LOGD("threads are down\n");

	// cleanup
	free_peer_list();

	// destroy all locks
	destroy_shutdown_lock();
	destroy_peer_list_lock();
	destroy_logger_lock();

	pthread_exit(NULL); // should be at end of main function
}
