#ifndef COMMAND_CLIENT_H
#define COMMAND_CLIENT_H

#include <sys/socket.h>

#include "message_queue.h"

// this thread should get new peers from the broadcast thread
// then it should try to connect to them and enumerate the files on the remote
// for each file that is found on the remote and not locally, it should create a download job for the
// file download thread

void* command_client_thread(void* tid);

// this is sent along as arguments with messages of type "peer_seen"
typedef struct message_peer_seen {
	char peer_id[6];
	struct sockaddr_storage address;
	struct timeval timestamp;
} message_data_peer_seen_type;

void command_client_thread_send_message(message_queue_entry_type* message);

#endif
