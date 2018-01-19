#ifndef COMMAND_CLIENT_H
#define COMMAND_CLIENT_H

#include <sys/socket.h>

#include "message_queue.h"

// this thread should get new peers from the broadcast thread
// then it should try to connect to them and enumerate the files on the remote
// for each file that is found on the remote and not locally, it should create a download job for the
// file download thread

/**
 * @brief This is the thread's main function. It is started from the main thread.
 *
 * \code{.c}
 * pthread_create(&command_client_thread_id, NULL, command_client_thread, (void*)0);
 * \endcode
 * @param user_data This parameter can be used to supply user data to the thread
 */
void* command_client_thread(void* user_data);

// this is sent along as arguments with messages of type "peer_seen"
/// This is a wrapper structure to send message arguments to the command client thread
typedef struct message_peer_seen {
	char peer_id[6]; //!< The id of the newly discovered peer
	struct sockaddr_storage address; //!< The address of the newly discovered peer
	struct timeval timestamp; //!< The time when the peer was discovered
} message_data_peer_seen_type;

/**
 * @brief This function is used to inform the command client that a new peer was discovered
 * @param message The message containing the job parameters.
 */
void command_client_thread_send_message(message_queue_entry_type* message);

#endif
