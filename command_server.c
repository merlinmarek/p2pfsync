#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "defines.h"
#include "file_client.h"
#include "logger.h"
#include "message_queue.h"
#include "shutdown.h"
#include "util.h"

#include "command_server.h"

// helper functions for this module
static void handle_client(int socketfd, char* receive_buffer, int received_bytes);

// static variables for this module
static message_queue_type* message_queue = NULL;

void command_server_thread_send_message(message_queue_entry_type* message) {
	message_queue_push(message_queue, message);
}

void* command_server_thread(void* user_data) {
	LOGD("started\n");

	// this has to be called otherwise this thread will not be able to receive any messages
	message_queue = message_queue_create_queue();

	int listener_socket = create_tcp_listener(COMMAND_LISTENER_PORT_STRING);
	if(listener_socket == -1) {
		LOGD("create_tcp_listener failed\n");
	}
	if(listen(listener_socket, 10) != 0) {
		LOGD("listen %s\n", strerror(errno));
	}

	LOGD("command server listenening @ %d\n", listener_socket);

	fd_set master_read_set;
	FD_ZERO(&master_read_set);
	FD_SET(listener_socket, &master_read_set);

	int maxfd = listener_socket;

	while(!get_shutdown()) {

	    fd_set read_set = master_read_set;
	    struct timeval timeout;
	    memset(&timeout, 0, sizeof(timeout));
	    timeout.tv_sec = 1; // block at maximum one second at a time
	    int success = select(maxfd + 1, &read_set, NULL, NULL, &timeout);
	    if(success == -1) {
	    	LOGD("select: %s\n", strerror(errno));
	    	continue;
	    }
	    if(success == 0) {
	    	// timed out
	    	continue;
	    }
	    // check which socket is ready
	    int socketfd;
	    for(socketfd = 0; socketfd <= maxfd; socketfd++) {
	    	if(FD_ISSET(socketfd, &read_set)) {
	    		// the socket is ready
	    		if(socketfd == listener_socket) {
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
                    int received_bytes = tcp_message_receive(socketfd, receive_buffer, sizeof(receive_buffer - 1), 5.0);
                    if(received_bytes == -1) {
                    	// error on recv
                    	// if this happens we want to close this socket and remove it from the master_fds
                    	LOGD("recv %s\n", strerror(errno));
                    	FD_CLR(socketfd, &master_read_set);
                    	continue;
                    }
                    if(received_bytes == 0) {
                    	// connection closed by remote
                    	FD_CLR(socketfd, &master_read_set);
                    	continue;
                    }
                    handle_client(socketfd, receive_buffer, received_bytes);
	    		}
	    	}
	    }
	    // if we reach this point we can accept an incoming connection
		sleep(1);
	}

	// cleanup
	// when we are done, we want to close all still open connections
	int socketfd;
	for(socketfd = 0; socketfd < maxfd; socketfd++) {
		if(FD_ISSET(socketfd, &master_read_set)) {
			close(socketfd);
		}
	}
	message_queue_free_queue(message_queue);
	message_queue = NULL;
	LOGD("ended\n");
	return NULL;
}

void handle_client(int socketfd, char* receive_buffer, int received_bytes) {
    receive_buffer[received_bytes] = ' '; // for strtok
    const char* delim = " ";
    const char* request_id = strtok(receive_buffer, delim);
    const char* request_path = strtok(NULL, delim);
    char* local_path = malloc(strlen(BASE_PATH) + strlen(request_path) + 1);
    memcpy(local_path, BASE_PATH, strlen(BASE_PATH));
    strcpy(local_path + strlen(BASE_PATH), request_path);
    //LOGD("id: %s, path: %s\n", request_id, local_path);

    // now we enumerate files in the requested directory and return them as csv
    char reply[8096 * 8];
    memset(reply, 0, sizeof(reply));

    struct stat info;

    if(lstat(local_path, &info) != 0 || !S_ISDIR(info.st_mode)) {
        // the directory does not exist or is not a directory
        strcpy(reply, "requested invalid directory");
    }
    else {
        // valid directory so list its contents
        DIR* directory;
        directory = opendir(local_path);
        if(!directory) {
            // error :/
        }
        int current_pos = 0;
        while(1) {
            struct dirent* entry;
            entry = readdir(directory);
            if(!entry) {
                // there are no more entrys so we break
                break;
            }
            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                // do not send current and parent directory
                continue;
            }
            if(entry->d_type != DT_DIR && entry->d_type != DT_REG) {
            	// we only want to deal with regular files and directories
            	continue;
            }
            char path_buffer[PATH_MAX];
            strcpy(path_buffer, local_path);
            strcat(path_buffer, "/");
            strcat(path_buffer, entry->d_name);
            if(lstat(path_buffer, &info) != 0 || (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode))) {
            	LOGD("invalid file/dir %s\n", path_buffer);
            }
            reply[current_pos++] = S_ISREG(info.st_mode) ? 'F' : 'D';
            reply[current_pos++] = '>'; // delimiter
            memcpy(reply + current_pos, entry->d_name, strlen(entry->d_name));
            current_pos += strlen(entry->d_name);
            reply[current_pos++] = '>';
            // now convert the changed date to a string representation so we do not have do deal with endianness
            char date_buffer[128];
            struct tm changed_time;
            gmtime_r(&info.st_ctim.tv_sec, &changed_time);
            strftime(date_buffer, sizeof(date_buffer), "%d.%m.%Y %a %T", &changed_time);
            memcpy(reply + current_pos, date_buffer, strlen(date_buffer));
            current_pos += strlen(date_buffer);
            reply[current_pos++] = '<'; // delimiter between file
        }
        reply[current_pos] = 0; // end of string instead of <
        //LOGD("sending %s\n", reply);
    }
    if(tcp_message_send(socketfd, reply, strlen(reply), 2.0) <= 0) {
    	LOGD("send %s\n", strerror(errno));
    }
    free(local_path);
}
