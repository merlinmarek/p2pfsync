#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "defines.h"
#include "logger.h"
#include "peerList.h"
#include "ipAddressList.h"
#include "broadcast.h"
#include "util.h"

void appendPeer(char id[6]);

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
Peer* findPeer(char id[6]) {
	Peer* peerIterator = NULL;
	for(peerIterator = peerList; peerIterator != NULL; peerIterator = peerIterator->nextPeer) {
		if(memcmp(peerIterator->id, id, 6) == 0) {
			break;
		}
	}
	return peerIterator;
}

void addIpToPeer(char id[6], struct sockaddr* ipAddress, struct timeval lastSeen) {
	pthread_mutex_lock(&peerListLock);
	Peer* peer = findPeer(id);
	if(peer == NULL) {
		LOGD("addIpToPeer peer not in list\n");
		appendPeer(id);
		peer = findPeer(id);
	}
	// the peer is valid
	addOrUpdateEntry(&peer->ipAddress, ipAddress, lastSeen);
	pthread_mutex_unlock(&peerListLock);
}

// internal helper function! MUST NOT LOCK MUTEX
void appendPeer(char id[6]) {
	if(findPeer(id) != NULL) {
		LOGD("peer already in list\n");
		return;
	}
	Peer* peer = (Peer*)malloc(sizeof(Peer));
	memset(peer, 0, sizeof(Peer));
	memcpy(&peer->id, id, 6);
	if(peerList == NULL) {
		// the list is empty
		peerList = peer;
		return;
	}
	Peer* peerIterator;
	for(peerIterator = peerList; peerIterator->nextPeer != NULL; peerIterator = peerIterator->nextPeer);
	peerIterator->nextPeer = peer;
}

void removePeer(char id[6]) {
	pthread_mutex_lock(&peerListLock);
	if(peerList == NULL) {
		// we cannot remove something from an empty list
		goto end;
	}
	Peer* peer = findPeer(id);
	if(peer == NULL) {
		LOGD("cannot remove peer, not in list\n");
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
		char hexBuffer[128];
		get_hex_string((unsigned char*)peerIterator->id, 6, hexBuffer, sizeof(hexBuffer));
		LOGD("[%s]\n", hexBuffer);
		printIpAddressList(&peerIterator->ipAddress);
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
