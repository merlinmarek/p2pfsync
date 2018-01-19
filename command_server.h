#ifndef COMMAND_SERVER_H
#define COMMAND_SERVER_H

// this module should only wait for connections and answer
// GET <path> request. It should list directory contents and send them to a remote
#include <sys/time.h>
#include <sys/socket.h>

#include "message_queue.h"

/**
 * @brief This is the thread's main function. It is started from the main thread.
 *
 * \code{.c}
 * pthread_create(&command_server_thread_id, NULL, command_server_thread, (void*)0);
 * \endcode
 * @param user_data This parameter can be used to supply user data to the thread
 */
void* command_server_thread(void* user_data);

/**
 * @brief This function could be used to send messages to the command server thread.
 *
 * This is currently not used.
 * @param message The message's parameters
 */
void command_server_thread_send_message(message_queue_entry_type* message);

#endif
