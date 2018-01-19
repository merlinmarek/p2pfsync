#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdlib.h>
#include <pthread.h>

// this data structure should be synchronized so we need mutexes for each
// queue as well... I dont know how this will play out...

/// This represents a single node in a message queue
typedef struct message_queue_entry {
	struct message_queue_entry* next; //!< A link to the next entry in the queue
	char* message_id; //!< A message id e.g. "peer discovered"
	void* arguments; //!< Additional arguments for a message
} message_queue_entry_type;

/// This represents a message queue
typedef struct {
	message_queue_entry_type* head; //!< The first entry of the message queue
	pthread_mutex_t mutex; //!< A mutex for this message queue to ensure thread safety
} message_queue_type;

// this function creates a new queue with a mutex and
// initializes the mutex
/**
 * @brief This function creates a new message queue and initializes its mutex
 * @return The memory address where the created queue resides
 */
message_queue_type* message_queue_create_queue();

// this function frees the queue and destroys the mutex
/**
 * @brief Frees the message queue and all messages currently queued up
 * @param message_queue A pointer to the message queue to free
 */
void message_queue_free_queue(message_queue_type* message_queue);

// synchronized function that enqueues a message
/**
 * @brief This function is used to append a message to a queue in a thread safe manner
 * @param message_queue A pointer to the message queue that the message should be appended to
 * @param message The message to append
 */
void message_queue_push(message_queue_type* message_queue, message_queue_entry_type* message);

// synchronized function that dequeues a message
/**
 * @brief This function removes the first element of a message queue and returns it.
 * @param message_queue The message queue to get the element from
 * @return The first element of the queue
 */
message_queue_entry_type* message_queue_pop(message_queue_type* message_queue);

// helper functions for messages
// THESE FUNCTIONS ARE NOT CALLED ON A SPECIFIC message_queue
// the create function allocates new memory and copies the id and the arguments
/**
 * @brief This function should be used to create new messages.
 *
 * It allocates the memory for a message and copies the provided data so the original message parameters are not affected
 * @param message_id The id of the message
 * @param arguments The additional arguments of the message
 * @param arguments_size The size of the arguments
 * @return
 */
message_queue_entry_type* message_queue_create_message(const char* message_id, const void* arguments, size_t arguments_size);

// the free function frees all allocated memory
/**
 * @brief This function frees the memory occupied by a message
 * @param message A pointer to the message to free
 */
void message_queue_free_message(message_queue_entry_type* message);

#endif
