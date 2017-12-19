#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include "util.h"
#include "ip_address_list.h"
#include "logger.h"

// internal helper function
ip_address_entry_type* find_entry(ip_address_entry_type** list, struct sockaddr* ip_address) {
	ip_address_entry_type* ip_address_iterator;
	for(ip_address_iterator = *list; ip_address_iterator != NULL; ip_address_iterator = ip_address_iterator->next_entry) {
		struct sockaddr* iterator_address = (struct sockaddr*)&ip_address_iterator->ip_address;
		if(iterator_address->sa_family == AF_INET) {
			// for ipv4 we need to compare the two addresses
			if(((struct sockaddr_in*)iterator_address)->sin_addr.s_addr == ((struct sockaddr_in*)ip_address)->sin_addr.s_addr) {
				// same address, so we have a match
				return ip_address_iterator;
			}
		}
		if(iterator_address->sa_family == AF_INET6) {
			// for ipv6 we need to compare 16 bytes
			void* given_ipv6 = (void*)&(((struct sockaddr_in6*)ip_address)->sin6_addr.__in6_u);
			void* iterator_ipv6 = (void*)&(((struct sockaddr_in6*)iterator_address)->sin6_addr.__in6_u);
			if(memcmp(given_ipv6, iterator_ipv6, 16) == 0) {
				// we have a match
				return ip_address_iterator;
			}
		}
	}
	return NULL;
}

void free_ip_address_list(ip_address_entry_type** list) {
	ip_address_entry_type* next_ip_address;
	while(*list != NULL) {
		next_ip_address = (*list)->next_entry;
		free((*list));
		*list = next_ip_address;
	}
}

void add_or_update_entry(ip_address_entry_type** list, struct sockaddr* ip_address, struct timeval last_seen) {
	if(*list == NULL) {
		// this is the first element
		ip_address_entry_type* new_entry = (ip_address_entry_type*)malloc(sizeof(ip_address_entry_type));
		memset(new_entry, 0, sizeof(ip_address_entry_type));
		memcpy(&new_entry->ip_address, ip_address, ip_address->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
		memcpy(&new_entry->last_seen, &last_seen, sizeof(struct timeval));
		*list = new_entry;
		return;
	}
	ip_address_entry_type* entry = find_entry(list, ip_address);
	if(entry == NULL) {
		// we need to add the entry
		ip_address_entry_type* last_entry;
		for(last_entry = *list; last_entry->next_entry != NULL; last_entry = last_entry->next_entry);
		// we now have the last Entry in lastEntry

		// create a new Entry, copy the data and apped it to the list
		ip_address_entry_type* new_entry = (ip_address_entry_type*)malloc(sizeof(ip_address_entry_type));
		memset(new_entry, 0, sizeof(ip_address_entry_type));
		memcpy(&new_entry->ip_address, ip_address, ip_address->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
		memcpy(&new_entry->last_seen, &last_seen, sizeof(struct timeval));
		last_entry->next_entry = new_entry;
	} else {
		// we only need to update the entry
		memcpy(&entry->last_seen, &last_seen, sizeof(struct timeval));
	}
}

void print_ip_address_list(ip_address_entry_type** list) {
	if(*list == NULL) {
		// the list is empty
		return;
	}
	ip_address_entry_type* ip_address_iterator;
	for(ip_address_iterator = *list; ip_address_iterator != NULL; ip_address_iterator = ip_address_iterator->next_entry) {
		// format is
		// \t[ipversion ip]
		char ip_buffer[128];
		get_ip_address_formatted((struct sockaddr*)&ip_address_iterator->ip_address, ip_buffer, sizeof(ip_buffer));
		LOGD("\t[%s]\n", ip_buffer);
	}
}
