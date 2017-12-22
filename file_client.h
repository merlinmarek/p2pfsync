#ifndef FILE_DOWNLOAD_H
#define FILE_DOWNLOAD_H

#include <limits.h>
#include <sys/socket.h>

#include "message_queue.h"

// this thread should only check its job queue and download each file that is in it
void* file_client_thread(void* tid);

// this is sent along as arguments with messages of type "download file"
typedef struct message_download_file {
	struct sockaddr_storage address;
	char file_path[PATH_MAX];
} message_data_download_file_type;

void file_client_thread_send_message(message_queue_entry_type* message);

#endif
