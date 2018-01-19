#ifndef FILE_UPLOAD_H
#define FILE_UPLOAD_H

#include "message_queue.h"

// this thread should only listen for incoming file requests and then send the files to remote peers
/**
 * @brief This is the thread's main function. It is started from the main thread.
 *
 * \code{.c}
 * pthread_create(&file_server_thread_id, NULL, file_server_thread, (void*)0);
 * \endcode
 * @param user_data This parameter can be used to supply user data to the thread
 */
void* file_server_thread(void* user_data);

/**
 * @brief This function could be used to send messages to the file server thread.
 *
 * This is currently not used.
 * @param message The message's parameters
 */
void file_server_thread_send_message(message_queue_entry_type* message);

#endif
