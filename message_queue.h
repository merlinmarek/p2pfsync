#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdlib.h>
#include <pthread.h>

// this data structure should be synchronized so we need mutexes for each
// queue as well... I dont know how this will play out...

typedef struct message_queue_entry {
	struct message_queue_entry* next;
	char* message_id;
	void* arguments;
} message_queue_entry_type;

typedef struct {
	message_queue_entry_type* head;
	pthread_mutex_t mutex;
} message_queue_type;

// this function creates a new queue with a mutex and
// initializes the mutex
message_queue_type* message_queue_create_queue();

// this function frees the queue and destroys the mutex
void message_queue_free_queue(message_queue_type* message_queue);

// synchronized function that enqueues a message
void message_queue_push(message_queue_type* message_queue, message_queue_entry_type* message);

// synchronized function that dequeues a message
message_queue_entry_type* message_queue_pop(message_queue_type* message_queue);

// helper functions for messages
// THESE FUNCTIONS ARE NOT CALLED ON A SPECIFIC message_queue
// the create function allocates new memory and copies the id and the arguments
message_queue_entry_type* message_queue_create_message(const char* message_id, const void* arguments, size_t arguments_size);
// the free function frees all allocated memory
void message_queue_free_message(message_queue_entry_type* message);

#endif
