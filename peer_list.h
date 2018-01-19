/**
 * @file peer_list.h
 * @brief This file provides a single thread safe list to store peers (id, ip addresses)
 */

#ifndef PEER_LIST_H
#define PEER_LIST_H

#include <sys/time.h>
#include <sys/socket.h>

/**
 * @brief This function initializes the mutex of the peer list. This should be called before first usage
 */
void initialize_peer_list_lock();

/**
 * @brief When the peer list is not needed anymore its mutex should be destroyed by calling this function.
 */
void destroy_peer_list_lock();

// addIpToPeer should automatically append the peer if it is not in the list
/**
 * @brief This function adds an ip address to a peer
 * @param id The id of the peer to add the ip address to
 * @param ip_address The ip address
 * @param last_seen The timestamp when the ip address was last seen
 */
void add_ip_to_peer(char id[6], struct sockaddr* ip_address, struct timeval last_seen);

/**
 * @brief Removes a peer from the list given its id
 * @param id The peer's id
 */
void remove_peer(char id[6]);

/**
 * @brief Checks whether a peer is already in the list given its id.
 * @param id The peer's id
 * @return 1 if the peer is in the list. Otherwise 0 is returned.
 */
int is_peer_in_list(char id[6]);

/**
 * @brief Gets the ip address of a peer.
 * @param id The peer's id
 * @param ip_address A memory location where the ip address should be copied to
 * @return 1 if the address was copied. 0 if not.
 */
int get_peer_ip_address(char id[6], struct sockaddr_storage* ip_address);

/**
 * @brief Prints out the peer list.
 */
void print_peer_list();

/**
 * @brief Frees the peer list.
 */
void free_peer_list();

#endif
