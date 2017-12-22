#ifndef BROADCAST_H
#define BROADCAST_H

#include "message_queue.h"

// this module only exposes the broadcastThread
// for this to work correctly the peerList has to be setup beforehand
void* broadcast_thread(void* tid);
void broadcast_thread_send_message(message_queue_entry_type* message);

#endif
