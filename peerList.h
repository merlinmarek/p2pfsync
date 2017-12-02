#ifndef PEER_LIST_H
#define PEER_LIST_H

// there will be a single synchronized peer list
// so here need to be mutexes as well

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
