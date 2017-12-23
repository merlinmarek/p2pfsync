#include <errno.h>
#include <fcntl.h>
#include <string.h>
#define __USE_XOPEN
#include <time.h> // __USE_XOPEN is needed otherwise strptime is not defined
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "defines.h"
#include "file_client.h"
#include "logger.h"
#include "shutdown.h"
#include "util.h"

#include "command_client.h"

// helper functions for this module
static void download_remote_directory(const int socketfd, const struct sockaddr* remote_address, const char* path);
static void print_peer_seen_data(message_data_peer_seen_type* message_data);

// static variables for this module
static message_queue_type* message_queue = NULL;

void command_client_thread_send_message(message_queue_entry_type* message) {
	message_queue_push(message_queue, message);
}

void* command_client_thread(void* tid) {
	LOGD("started\n");
	// this has to be called otherwise this thread will not be able to receive any messages
	message_queue = message_queue_create_queue();
	while(!get_shutdown()) {
		// handle messages sent by other threads
		message_queue_entry_type* message;
		while((message = message_queue_pop(message_queue)) != NULL) {
			LOGD("received message: %s\n", message->message_id);
			if(strcmp(message->message_id, "peer_seen") == 0) {
				// extract the peer_seen data from the message
				message_data_peer_seen_type* peer_seen_data = (message_data_peer_seen_type*)message->arguments;
				print_peer_seen_data(peer_seen_data);
				int commandfd = connect_with_timeout((struct sockaddr*)&peer_seen_data->address, COMMAND_LISTENER_PORT, 5);
				if(commandfd == -1) {
					// the connection failed
					// continue with the next message
					continue;
				}
				download_remote_directory(commandfd, (const struct sockaddr*)&peer_seen_data->address, "/");
            	close(commandfd);
			}
			else {
				LOGD("\tunkown message id :(\n");
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

void download_remote_directory(const int socketfd, const struct sockaddr* remote_address, const char* path) {
	char request_buffer[PATH_MAX];
	memset(request_buffer, 0, sizeof(request_buffer));

	// request the directory
	strcpy(request_buffer, "GET ");
	strcat(request_buffer, path);

	size_t sent_bytes = tcp_message_send(socketfd, request_buffer, strlen(request_buffer), 0);
    if(sent_bytes < 0) {
        LOGE("send %s\n", strerror(errno));
        return;
    }
    // this buffer size should be big enough to hold all files in a directory, this should probably be dynamic
    char receive_buffer[8096 * 8];
    int received_bytes = tcp_message_receive(socketfd, receive_buffer, sizeof(receive_buffer), 2.0);
    const char* delim = "<";
    receive_buffer[received_bytes] = 0;
    char* entry_token = NULL;
    char* entry = strtok_r(receive_buffer, delim, &entry_token);
    while(entry != NULL) {
        // here are all entries
        // we should act upon them
        // directories should not be considered for now, only files
        // the format is
        // F/D>name>lastchangedtime the delimiter is >
        // so we strtok again
        char* element_token = NULL;
        const char* type = strtok_r(entry, ">", &element_token);
        const char* name = strtok_r(NULL, ">", &element_token);
        const char* last_changed = strtok_r(NULL, ">", &element_token);

        if(type == NULL || name == NULL || last_changed == NULL || (strcmp(type, "D") != 0 && strcmp(type, "F") != 0)) {
            LOGD("entry not recognized %s\n", entry);
        } else {
            struct tm remote_time_utc;
            memset(&remote_time_utc, 0, sizeof(remote_time_utc));
            strptime(last_changed, "%d.%m.%Y %a %T", &remote_time_utc);

            char date_buffer[128];
            strftime(date_buffer, sizeof(date_buffer), "%d.%m.%Y %a %T", &remote_time_utc);

            LOGD("%s %s %s\n", type, name, date_buffer);

            if(strcmp(type, "D") == 0) {
            	// this is a directory, so we want to download that as well
            	// reuse the request_buffer
            	// for directories we need to append a /
                strcpy(request_buffer, path);
                strcat(request_buffer, name);
            	strcat(request_buffer, "/");
            	download_remote_directory(socketfd, remote_address, request_buffer);
            }
            else {
            	// this is a file, so we need to check whether it is locally present and the download it
            	// so check now if the file exists locally
            	// when testing for the local path we have to prepend the base directory
            	strcpy(request_buffer, BASE_PATH);
                strcat(request_buffer, path);
                strcat(request_buffer, name);

            	struct stat info;
            	memset(&info, 0, sizeof(info));
            	if(lstat(request_buffer, &info) != 0 || !S_ISREG(info.st_mode)) {
            		// not found or not a file
            		// so we need to download it
            		LOGI("file not present: %s\n", request_buffer);
            		// create a download job for the file
            		// now the path has to be again relative to BASE_PATH
                    message_data_download_file_type download_file_data;
                    memset(&download_file_data, 0, sizeof(download_file_data));
                    strcpy(download_file_data.file_path, request_buffer + strlen(BASE_PATH));
                    memcpy(&download_file_data.address, remote_address, sizeof(struct sockaddr_storage));
                    message_queue_entry_type* message = message_queue_create_message("download_file", (void*)&download_file_data, sizeof(download_file_data));
                    file_client_thread_send_message(message);
            	}
            }
        }
        entry = strtok_r(NULL, delim, &entry_token);
    }
}

void print_peer_seen_data(message_data_peer_seen_type* message_data) {
    char id_buffer[13];
    get_hex_string((unsigned char*)message_data->peer_id, 6, id_buffer, sizeof(id_buffer));
    char ip_buffer[128];
    get_ip_address_string_prefixed((struct sockaddr*)&message_data->address, ip_buffer, sizeof(ip_buffer));
    char date_buffer[128];
    strftime(date_buffer, sizeof(date_buffer), "%d.%m.%G %a %T", localtime(&message_data->timestamp.tv_sec));
    LOGD("\tid: %s, ip: %s, seen: %s\n", id_buffer, ip_buffer, date_buffer);

}
