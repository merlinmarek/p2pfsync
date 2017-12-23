#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "defines.h"
#include "logger.h"
#include "shutdown.h"
#include "util.h"

#include "file_client.h"

// helper functions for this module
static void download_file(struct sockaddr* address, const char* file_path);

// static variables for this module
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

				download_file((struct sockaddr*)&download_file_data->address, download_file_data->file_path);
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

void download_file(struct sockaddr* address, const char* file_path) {
    char ip_buffer[128];
    get_ip_address_string_prefixed(address, ip_buffer, sizeof(ip_buffer));
    LOGI("downloading %s from %s\n", file_path, ip_buffer);

    // create a tcp connection to the remote
    int socketfd = connect_with_timeout(address, FILE_LISTENER_PORT, 5);
    if(socketfd == -1) {
    	LOGE("connect_with_timeout failed\n");
    	return;
    }
    // the request should look like GET <path>
    char request_buffer[PATH_MAX];
    strcpy(request_buffer, "GET ");
    strcat(request_buffer, file_path);
    int send_return = tcp_message_send(socketfd, request_buffer, strlen(request_buffer), 5.0);
    if(send_return <= 0) {
    	LOGE("send_tcp_message failed\n");
    	close(socketfd);
    	return;
    }
    // maximum 20mb file for now...
    char* file_buffer = (char*)malloc(20000000);
    if(file_buffer == NULL) {
    	LOGE("out of memory :/\n");
    	close(socketfd);
    	return;
    }
    int recv_return = tcp_message_receive(socketfd, file_buffer, 20000000, 20000.0);
    if(recv_return <= 0) {
    	LOGE("receive_tcp_message failed\n");
    	close(socketfd);
    	free(file_buffer);
    	return;
    }
    // we should have the remote file in memory now
    LOGI("remote file: %s has contents: %*.*s", file_path, 0, recv_return, file_buffer);
    close(socketfd);
    free(file_buffer);
}
