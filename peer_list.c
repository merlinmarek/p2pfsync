#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "defines.h"
#include "logger.h"
#include "peer_list.h"
#include "ip_address_list.h"
#include "broadcast.h"
#include "util.h"

void append_peer(char id[6]);

typedef struct peer {
	// link to the next peer
	struct peer* next_peer;

	// id of the peer
	char id[6];

	// a list of ip addresses
	struct ip_address_entry* ip_address;
} peer_t;

static pthread_mutex_t peer_list_lock;

void initialize_peer_list_lock() {
	if(pthread_mutex_init(&peer_list_lock, NULL) != 0) {
		printf("pthread_mutex_init failed\n");
	}
}

void destroy_peer_list_lock() {
	if(pthread_mutex_destroy(&peer_list_lock) != 0) {
		printf("pthread_mutex_destory failed\n");
	}
}

static peer_t* peer_list = NULL;

// helper function
// this function MUST NOT LOCK THE MUTEX as it is called by the other locking functions
peer_t* find_peer(char id[6]) {
	peer_t* peer_iterator = NULL;
	for(peer_iterator = peer_list; peer_iterator != NULL; peer_iterator = peer_iterator->next_peer) {
		if(memcmp(peer_iterator->id, id, 6) == 0) {
			break;
		}
	}
	return peer_iterator;
}

void add_ip_to_peer(char id[6], struct sockaddr* ip_address, struct timeval last_seen) {
	pthread_mutex_lock(&peer_list_lock);
	peer_t* peer = find_peer(id);
	if(peer == NULL) {
		LOGD("peer not in list\n");
		append_peer(id);
		peer = find_peer(id);
	}
	// the peer is valid
	add_or_update_entry(&peer->ip_address, ip_address, last_seen);
	pthread_mutex_unlock(&peer_list_lock);
}

// internal helper function! MUST NOT LOCK MUTEX
void append_peer(char id[6]) {
	if(find_peer(id) != NULL) {
		LOGD("peer already in list\n");
		return;
	}
	peer_t* peer = (peer_t*)malloc(sizeof(peer_t));
	memset(peer, 0, sizeof(peer_t));
	memcpy(&peer->id, id, 6);
	if(peer_list == NULL) {
		// the list is empty
		peer_list = peer;
		return;
	}
	peer_t* peer_iterator;
	for(peer_iterator = peer_list; peer_iterator->next_peer != NULL; peer_iterator = peer_iterator->next_peer);
	peer_iterator->next_peer = peer;
}

void remove_peer(char id[6]) {
	pthread_mutex_lock(&peer_list_lock);
	if(peer_list == NULL) {
		// we cannot remove something from an empty list
		goto end;
	}
	peer_t* peer = find_peer(id);
	if(peer == NULL) {
		LOGD("cannot remove peer, not in list\n");
		goto end;
	}
	if(peer_list == peer) {
		// we want to remove the head
		peer_list = peer->next_peer;
		free(peer);
		goto end;
	}

	// otherwise we search for the peer in the list
	peer_t* peer_iterator;
	peer_t* previous_peer;
	for(peer_iterator = peer_list; (peer_iterator != peer && peer_iterator != NULL); previous_peer = peer_iterator, peer_iterator = peer_iterator->next_peer);
	previous_peer->next_peer = peer_iterator->next_peer;
	free(peer_iterator);
end:
	pthread_mutex_unlock(&peer_list_lock);
}

void print_peer_list() {
	pthread_mutex_lock(&peer_list_lock);
	peer_t* peer_iterator;
	for(peer_iterator = peer_list; peer_iterator != NULL; peer_iterator = peer_iterator->next_peer) {
		char hex_buffer[128];
		get_hex_string((unsigned char*)peer_iterator->id, 6, hex_buffer, sizeof(hex_buffer));
		LOGD("[%s]\n", hex_buffer);
		print_ip_address_list(&peer_iterator->ip_address);
	}
	pthread_mutex_unlock(&peer_list_lock);
}

void free_peer_list() {
	pthread_mutex_lock(&peer_list_lock);
	peer_t* next_peer;
	while(peer_list != NULL) {
		next_peer = peer_list->next_peer;
		free(peer_list);
		peer_list = next_peer;
	}
	pthread_mutex_unlock(&peer_list_lock);
}
