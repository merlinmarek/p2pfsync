#ifndef IP_ADDRESS_LIST_H
#define IP_ADDRESS_LIST_H

#include <sys/time.h>
#include <sys/socket.h>

typedef struct ip_address_entry {
	struct ip_address_entry* next_entry;
	struct sockaddr_storage ip_address;
	struct timeval last_seen;
} ip_address_entry_type;

void free_ip_address_list(ip_address_entry_type** list);
void add_or_update_entry(ip_address_entry_type** list, struct sockaddr* ip_address, struct timeval last_seen);

// this prefers ipv6 addresses over ipv4
// if no address is found it returns 0 otherwise 1
int get_best_address(ip_address_entry_type** list, struct sockaddr_storage* ip_address);
// convenient if list is always **
void print_ip_address_list(ip_address_entry_type** list);

#endif
