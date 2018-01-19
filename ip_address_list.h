#ifndef IP_ADDRESS_LIST_H
#define IP_ADDRESS_LIST_H

#include <sys/time.h>
#include <sys/socket.h>

/// A single entry or the head of an ip address list
typedef struct ip_address_entry {
	struct ip_address_entry* next_entry; //!< A link to the next entry in the list
	struct sockaddr_storage ip_address; //!< The ip address of a peer
	struct timeval last_seen; //!< When was this ip address last seen
} ip_address_entry_type;

/**
 * @brief Frees the memory an ip address list occupies given its head node.
 * @param list A pointer to the address where the head of the list resides
 */
void free_ip_address_list(ip_address_entry_type** list);

/**
 * @brief Insert an ip address into the list. If the ip is already in the list its timestamp is updated.
 * @param list A pointer to the address where the head of the list resides
 * @param ip_address The ip address to insert
 * @param last_seen When was this ip last seen
 */
void add_or_update_entry(ip_address_entry_type** list, struct sockaddr* ip_address, struct timeval last_seen);

// this prefers ipv6 addresses over ipv4
// if no address is found it returns 0 otherwise 1
/**
 * @brief This gets the best address out of an ip address list.
 *
 * For each discovered peer there is one ip address list. It contains all the address that belong to this peer.
 * These might be many e.g. one link over ethernet, another over wifi, etc. and both IPv4 and IPv6.
 * This function gives the "best" which in this case is the first IPv6 address. If no IPv6 address is found
 * an IPv4 address is returned instead. If no address is found the function returns 0.
 * @param list A pointer to the address where the head of the list resides
 * @param ip_address A pointer to a memory location where the "best" ip should be copied to
 * @return
 */
int get_best_address(ip_address_entry_type** list, struct sockaddr_storage* ip_address);

// convenient if list is always **
/**
 * @brief Print out an ip address list given its head node using the logger module
 * @param list A pointer to the address where the head of the list resides
 */
void print_ip_address_list(ip_address_entry_type** list);

#endif
