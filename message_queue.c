#include <string.h>

#include "logger.h"
#include "message_queue.h"

message_queue_type* message_queue_create_queue() {
	message_queue_type* new_queue = (message_queue_type*)malloc(sizeof(message_queue_type));
	memset(new_queue, 0, sizeof(message_queue_type));
	if(pthread_mutex_init(&new_queue->mutex, NULL) != 0) {
		LOGE("pthread_mutex_init failed\n");
	}
	return new_queue;
}

// the callers have to make sure that this is not called while another thread wants to use
// synchronized functions...
void message_queue_free_queue(message_queue_type* message_queue) {
	// first we free all messages in the list
	message_queue_entry_type* message_iterator = message_queue->head;
	while(message_iterator != NULL) {
		message_queue_entry_type* saved_next = message_iterator->next;
		message_queue_free_message(message_iterator);
		message_iterator = saved_next;
	}
	// last we free the message_queue
	if(pthread_mutex_destroy(&message_queue->mutex) != 0) {
		LOGE("pthread_mutex_destroy failed\n");
	}
	free(message_queue);
}

// messages coming in here should always have next = NULL
void message_queue_push(message_queue_type* message_queue, message_queue_entry_type* message) {
	pthread_mutex_lock(&message_queue->mutex);
	// find the end and append the message;
	if(message_queue->head == NULL) {
		// queue is empty, special case
		message_queue->head = message;
	}
	else {
		// apped at end
		message_queue_entry_type* iterator;
		for(iterator = message_queue->head; iterator->next != NULL; iterator = iterator->next);
		iterator->next = message;
	}
	pthread_mutex_unlock(&message_queue->mutex);
}
message_queue_entry_type* message_queue_pop(message_queue_type* message_queue) {
	pthread_mutex_lock(&message_queue->mutex);
	message_queue_entry_type* message = NULL;
	if(message_queue->head != NULL) {
		message = message_queue->head;
		message_queue->head = message_queue->head->next;
	}
	pthread_mutex_unlock(&message_queue->mutex);
	return message;
}

// helper functions for messages
// the create function allocates new memory and copies the id and the arguments
message_queue_entry_type* message_queue_create_message(const char* message_id, const void* arguments, size_t arguments_size) {
	// this does not need to be synchronized
	// it does not operate directly on the queue
	message_queue_entry_type* new_message = (message_queue_entry_type*)malloc(sizeof(message_queue_entry_type));
	memset(new_message, 0, sizeof(message_queue_entry_type));
	// now we need to copy the message as well as the argument
	char* copied_id = (char*)malloc(strlen(message_id) + 1);
	strcpy(copied_id, message_id);
	new_message->message_id = copied_id;

	if(arguments != NULL) {
        void* copied_arguments = (char*)malloc(arguments_size);
        memcpy(copied_arguments, arguments, arguments_size);
        new_message->arguments = copied_arguments;
	}
	return new_message;
}

// the free function frees all allocated memory
void message_queue_free_message(message_queue_entry_type* message) {
	free(message->message_id);
	if(message->arguments != NULL) {
		free(message->arguments);
	}
	free(message);
}
