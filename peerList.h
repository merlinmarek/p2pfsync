#ifndef PEER_LIST_H
#define PEER_LIST_H

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
void initializePeerListLock();
void destroyPeerListLock();

// peers should be uniquely identified by their ip address
// so the functions only take an ip address as parameter

void appendPeer(unsigned char ipVersion, const char* ipAddress);
void removePeer(unsigned char ipVersion, const char* ipAddress);
void printPeerList();
void freePeerList();

#endif
