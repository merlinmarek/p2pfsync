/**
 * @file broadcast.h
 * @brief This is the broadcast module.
 *
 * This module has its own thread and is responsible for managing the peer discovery.
 * The thread periodically sends a broadcast packet to the local network containing the peer id. If a
 * broadcast packet of another peer is received, the broadcast thread does two things. First it sends
 * back a response so the discovering peer knows about this peer as well. Second a "peer seen" message
 * is generated an send to the command client thread so it can checkout the files that the remote peer
 * has.
 */

#ifndef BROADCAST_H
#define BROADCAST_H

#include "message_queue.h"

/**
 * @brief This is the thread's main function. It is started from the main thread.
 *
 * \code{.c}
 * pthread_create(&broadcast_thread_id, NULL, broadcast_thread, (void*)0);
 * \endcode
 * @param user_data This parameter can be used to supply user data to the thread
 */
void* broadcast_thread(void* user_data);

/**
 * @brief This function could be used to send messages to the broadcast thread.
 *
 * This is currently not used.
 * @param message The message's parameters
 */
void broadcast_thread_send_message(message_queue_entry_type* message);

#endif
