#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "defines.h"
#include "logger.h"
#include "shutdown.h"
#include "util.h"

#include "file_server.h"

// helper functions for this module
static void handle_client(int socketfd, char* receive_buffer, size_t received_bytes);

// static variables for this module
static message_queue_type* message_queue = NULL;

void file_server_thread_send_message(message_queue_entry_type* message) {
	message_queue_push(message_queue, message);
}

void* file_server_thread(void* user_data) {
	LOGD("started\n");
	// this has to be called otherwise this thread will not be able to receive any messages
	message_queue = message_queue_create_queue();

	int listener_socket = create_tcp_listener(FILE_LISTENER_PORT_STRING);
	if(listener_socket == -1) {
		LOGE("create_tcp_listener failed\n");
	}
	if(listen(listener_socket, 10) != 0) {
		LOGD("listen %s\n", strerror(errno));
	}
	LOGD("file server is listening @ %d\n", listener_socket);

	fd_set master_read_set;
	FD_ZERO(&master_read_set);
	FD_SET(listener_socket, &master_read_set);
	int maxfd = listener_socket;

	while(!get_shutdown()) {
		// handle messages sent by other threads
/*		message_queue_entry_type* message;
		while((message = message_queue_pop(message_queue)) != NULL) {
			LOGD("received message: %s\n", message->message_id);
			// free the message
			message_queue_free_message(message);
		}
		*/
		struct timeval timeout;
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		fd_set read_set = master_read_set;
		int select_return = select(maxfd + 1, &read_set, NULL, NULL, &timeout);
		if(select_return == -1) {
			LOGE("select: %s\n", strerror(errno));
			continue;
		}
		if(select_return == 0) {
			// timeout
			continue;
		}
		int socketfd;
		for(socketfd = 0; socketfd <= maxfd; socketfd++) {
			if(!FD_ISSET(socketfd, &read_set)) {
				continue;
			}
            if(socketfd == listener_socket) {
            	//LOGD("file server accepting...\n");
                // we are ready to accept a new client
                struct sockaddr_storage other_address;
                socklen_t other_length = sizeof(other_address);

                int remotefd = accept(listener_socket, (struct sockaddr*)&other_address, &other_length);
                if(remotefd == -1) {
                    // accept failed
                    LOGD("accept %s\n", strerror(errno));
                    continue;
                }
                FD_SET(remotefd, &master_read_set);
                if(remotefd > maxfd) {
                    // keep track of the maximum socket number
                    maxfd = remotefd;
                }
            } else {
                // this is a regular client socket that either closed the connection or wants something from us
                char receive_buffer[1024];
                int received_bytes = tcp_message_receive(socketfd, receive_buffer, sizeof(receive_buffer - 1), 0);
                if(received_bytes == -1) {
                    // error on recv
                    // if this happens we want to close this socket and remove it from the master_fds
                    LOGD("recv %s\n", strerror(errno));
                    FD_CLR(socketfd, &master_read_set);
                    continue;
                }
                if(received_bytes == 0) {
                    // connection closed by remote
                	LOGD("file server received 0 bytes...\n");
                    FD_CLR(socketfd, &master_read_set);
                    continue;
                }
                FD_CLR(socketfd, &master_read_set);
                handle_client(socketfd, receive_buffer, received_bytes);
                close(socketfd);
            }
		}
		sleep(1);
	}
	// cleanup
	message_queue_free_queue(message_queue);
	message_queue = NULL;
	LOGD("ended\n");
	return NULL;
}

void handle_client(int socketfd, char* receive_buffer, size_t received_bytes) {
    receive_buffer[received_bytes] = '>'; // for strtok
    const char* request_id = strtok(receive_buffer, " ");
    char* request_path = strtok(NULL, ">");
    if(request_id == NULL) {
    	LOGE("invalid request: %s\n", request_id);
    	return;
    }
    char* local_path = malloc(strlen(BASE_PATH) + strlen(request_path) + 1);
    memcpy(local_path, BASE_PATH, strlen(BASE_PATH));
    strcpy(local_path + strlen(BASE_PATH), request_path);

    struct stat info;
    if(lstat(local_path, &info) != 0 || !S_ISREG(info.st_mode)) {
    	// the path does not point to a file
    	LOGD("not a file: %s\n", local_path);
    	char reply[1000];
        strcpy(reply, "requested invalid directory");
        if(tcp_message_send(socketfd, reply, strlen(reply), 2.0) <= 0) {
            LOGD("send %s\n", strerror(errno));
        }
    }
    else {
        char* file_buffer = (char*)malloc(info.st_size);
        int file = open(local_path, O_RDONLY);
        if(file == -1) {
        	LOGD("%s could not be opened!\n", local_path);
        	free(file_buffer); // avoid memory leaks
        	close(file);
        }
        else if(read(file, file_buffer, info.st_size) != info.st_size) {
        	// we want to have the whole file in memory
        	// if this is not possible there is nothing we can do
        	LOGD("%s could not be read!\n", local_path);
        	close(file);
        	free(file_buffer);
        }
        // the file is in memory now
        //LOGD("sending %s\n", file_buffer);
        if(tcp_message_send(socketfd, file_buffer, info.st_size, 20.0) <= 0) {
        	LOGD("send_tcp_message failed\n");
        }
    }
    free(local_path);
}
