#ifndef FILE_DOWNLOAD_H
#define FILE_DOWNLOAD_H

#include <limits.h>
#include <sys/socket.h>

#include "message_queue.h"

/**
 * @brief This is the thread's main function. It is started from the main thread.
 *
 * \code{.c}
 * pthread_create(&file_client_thread_id, NULL, file_client_thread, (void*)0);
 * \endcode
 * @param user_data This parameter can be used to supply user data to the thread
 */
void* file_client_thread(void* user_data);

 /// This is a wrapper structure to send message arguments to the file client thread.
typedef struct message_download_file {
	struct sockaddr_storage address; //!< The address where the file resides
	char file_path[PATH_MAX]; //!< The path of the file to download
} message_data_download_file_type;

/**
 * @brief This function is used to add a new download job to the file client's queue
 * @param message The message containing the job parameters
 */
void file_client_thread_send_message(message_queue_entry_type* message);

#endif
