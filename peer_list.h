#ifndef PEER_LIST_H
#define PEER_LIST_H

#include <sys/time.h>
#include <sys/socket.h>

// there will be a single synchronized peer list
// so here need to be mutexes as well
// this list has to do a lot of work
// it should be able to accept an id and an ip address and
// figure out where to put it:
// the id identifies one peer and all ip addresses that belong to one peer
// should be placed in the peer struct. For this the data structure has to be reworked
// there should be a list of ip addresses associated with a peer
// the list should also keep track of how many peers there are and on how many ip
// addresses a peer has
// later it should also be able to manage the known files of a peer by mantaining some kind
// of list/tree structure that resembles a file system
// for now a peer should have an id and a list of ip addresses

// this list is the synchronization point for the different threads, so all operations should
// be implemented as fast as possible, because all functions are protected by mutex guards!!

// the broadcast thread adds available ip addresses
// the command thread queries available ip addresses and newly discovered peers

// these mutex functions MUST be called
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
