#include <unistd.h>

#include "logger.h"
#include "shutdown.h"

#include "file_server.h"

static message_queue_type* message_queue = NULL;

void file_server_thread_send_message(message_queue_entry_type* message) {
	message_queue_push(message_queue, message);
}

void* file_server_thread(void* tid) {
	LOGD("started\n");
	// this has to be called otherwise this thread will not be able to receive any messages
	message_queue = message_queue_create_queue();
	while(!get_shutdown()) {
		// handle messages sent by other threads
		message_queue_entry_type* message;
		while((message = message_queue_pop(message_queue)) != NULL) {
			LOGD("received message: %s\n", message->message_id);
			// free the message
			message_queue_free_message(message);
		}
		sleep(1);
	}
	// cleanup
	message_queue_free_queue(message_queue);
	message_queue = NULL;
	LOGD("ended\n");
	return NULL;
}
