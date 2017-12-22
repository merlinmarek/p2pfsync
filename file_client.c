#include <string.h>
#include <unistd.h>

#include "logger.h"
#include "shutdown.h"
#include "util.h"

#include "file_client.h"

static message_queue_type* message_queue = NULL;

void file_client_thread_send_message(message_queue_entry_type* message) {
	message_queue_push(message_queue, message);
}

void* file_client_thread(void* tid) {
	LOGD("started\n");
	// this has to be called otherwise this thread will not be able to receive any messages
	message_queue = message_queue_create_queue();
	while(!get_shutdown()) {
		// handle messages sent by other threads
		message_queue_entry_type* message;
		while((message = message_queue_pop(message_queue)) != NULL) {
			LOGD("received message: %s\n", message->message_id);
			if(strcmp(message->message_id, "download_file") == 0) {
				// extract the download_file_data from the message
				message_data_download_file_type* download_file_data = (message_data_download_file_type*)message->arguments;

				char ip_buffer[128];
				get_ip_address_formatted((struct sockaddr*)&download_file_data->address, ip_buffer, sizeof(ip_buffer));

				LOGI("downloading %s from %s\n", download_file_data->file_path, ip_buffer);
			}
			else {
				LOGD("unkown message id :(\n");
			}
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
