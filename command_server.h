#ifndef COMMAND_SERVER_H
#define COMMAND_SERVER_H

// this module should only wait for connections and answer
// GET <path> request. It should list directory contents and send them to a remote
#include <sys/time.h>
#include <sys/socket.h>

#include "message_queue.h"

// for this to work correctly the peerList has to be setup beforehand
void* command_server_thread(void* tid);

void command_server_thread_send_message(message_queue_entry_type* message);

#endif
