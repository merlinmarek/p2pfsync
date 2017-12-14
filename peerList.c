#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "defines.h"
#include "logger.h"
#include "peerList.h"
#include "ipAddressList.h"

typedef struct peer {
	// link to the next peer
	struct peer* nextPeer;

	// id of the peer
	char id[6];

	// a list of ip addresses
	struct ipAddressEntry* ipAddress;
} Peer;

static pthread_mutex_t peerListLock;

void initializePeerListLock() {
	if(pthread_mutex_init(&peerListLock, NULL) != 0) {
		printf("pthread_mutex_init failed\n");
	}
}

void destroyPeerListLock() {
	if(pthread_mutex_destroy(&peerListLock) != 0) {
		printf("pthread_mutex_destory failed\n");
	}
}

static Peer* peerList = NULL;

// helper function
// this function MUST NOT LOCK THE MUTEX as it is called by the other locking functions
Peer* findPeer(unsigned char ipVersion, const char* ipAddress) {
	Peer* peerIterator = NULL;
	for(peerIterator = peerList; peerIterator != NULL; peerIterator = peerIterator->nextPeer) {
		/*if(peerIterator->ipVersion == ipVersion && memcmp(peerIterator->ipv4address, ipAddress, ipVersion == 4 ? 4 : 16) == 0) {
			break;
		}
		*/
	}
	return peerIterator;
}

void appendPeer(unsigned char ipVersion, const char* ipAddress) {
	pthread_mutex_lock(&peerListLock);
	if(findPeer(ipVersion, ipAddress) != NULL) {
		logger("peer already in list\n");
		goto end;
	}
	Peer* peer = (Peer*)malloc(sizeof(Peer));
	memset(peer, 0, sizeof(Peer));
	//peer->ipVersion = ipVersion;
	if(peerList == NULL) {
		// the list is empty
		peerList = peer;
		goto end;
	}
	Peer* peerIterator;
	for(peerIterator = peerList; peerIterator->nextPeer != NULL; peerIterator = peerIterator->nextPeer);
	peerIterator->nextPeer = peer;
end: // this uses gotos so that the mutex guards can always be at the start an end of a function
	pthread_mutex_unlock(&peerListLock);
}

void removePeer(unsigned char ipVersion, const char* ipAddress) {
	pthread_mutex_lock(&peerListLock);
	if(peerList == NULL) {
		// we cannot remove something from an empty list
		goto end;
	}
	Peer* peer = findPeer(ipVersion, ipAddress);
	if(peer == NULL) {
		logger("cannot remove peer, not in list\n");
		goto end;
	}
	if(peerList == peer) {
		// we want to remove the head
		peerList = peer->nextPeer;
		free(peer);
		goto end;
	}

	// otherwise we search for the peer in the list
	Peer* peerIterator;
	Peer* previousPeer;
	for(peerIterator = peerList; (peerIterator != peer && peerIterator != NULL); previousPeer = peerIterator, peerIterator = peerIterator->nextPeer);
	previousPeer->nextPeer = peerIterator->nextPeer;
	free(peerIterator);
end:
	pthread_mutex_unlock(&peerListLock);
}

void printPeerList() {
	pthread_mutex_lock(&peerListLock);
	Peer* peerIterator;
	for(peerIterator = peerList; peerIterator != NULL; peerIterator = peerIterator->nextPeer) {
		//logger("peer: %s\n", peerIterator->ipv4address);
	}
	pthread_mutex_unlock(&peerListLock);
}

void freePeerList() {
	pthread_mutex_lock(&peerListLock);
	Peer* nextPeer;
	while(peerList != NULL) {
		nextPeer = peerList->nextPeer;
		free(peerList);
		peerList = nextPeer;
	}
	pthread_mutex_unlock(&peerListLock);
}
