#ifndef FILE_UPLOAD_H
#define FILE_UPLOAD_H

#include "message_queue.h"

// this thread should only listen for incoming file requests and then send the files to remote peers
void* file_server_thread(void* tid);

void file_server_thread_send_message(message_queue_entry_type* message);

#endif
