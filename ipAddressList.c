#include "ipAddressList.h"
#include "logger.h"
#include "socketUtil.h"

#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

// this list needs a little more thinking than the peerList
// because there will be more than one instance of it.
// i am not sure if mutexes are needed. This should probably be
// guarded by the peerlist functions. In this cases functions
// may never return addresses of values in the list but only
// copies. The threads should be completely shielded from this!
// ALL FUNCTIONS MUST ONLY BE CALLED BY THE PEER LIST
// ipAddressList.h MUST NOT BE INCLUDED IN ANY FILES EXCEPT
// peerList.h/c

// IMPORTANT
// some functions may rely on the peers having (approximately)
// the same clock time. This is especially important for file
// changed times. atm I dont know how to work around that...

// internal helper function
IpAddressEntry* findEntry(IpAddressEntry** list, struct sockaddr* ipAddress) {
	IpAddressEntry* ipAddressIterator;
	for(ipAddressIterator = *list; ipAddressIterator != NULL; ipAddressIterator = ipAddressIterator->nextEntry) {
		struct sockaddr* iteratorAddress = (struct sockaddr*)&ipAddressIterator->ipAddress;
		if(iteratorAddress->sa_family == AF_INET) {
			// for ipv4 we need to compare the two addresses
			if(((struct sockaddr_in*)iteratorAddress)->sin_addr.s_addr == ((struct sockaddr_in*)ipAddress)->sin_addr.s_addr) {
				// same address, so we have a match
				return ipAddressIterator;
			}
		}
		if(iteratorAddress->sa_family == AF_INET6) {
			// for ipv6 we need to compare 16 bytes
			void* givenIpv6 = (void*)&(((struct sockaddr_in6*)ipAddress)->sin6_addr.__in6_u);
			void* iteratorIpv6 = (void*)&(((struct sockaddr_in6*)iteratorAddress)->sin6_addr.__in6_u);
			if(memcmp(givenIpv6, iteratorIpv6, 16) == 0) {
				// we have a match
				return ipAddressIterator;
			}
		}
	}
	return NULL;
}

void freeIpAddressList(IpAddressEntry** list) {
	IpAddressEntry* nextIpAddress;
	while(*list != NULL) {
		nextIpAddress = (*list)->nextEntry;
		free((*list));
		*list = nextIpAddress;
	}
}

void addOrUpdateEntry(IpAddressEntry** list, struct sockaddr* ipAddress, struct timeval lastSeen) {
	//logger("addorupdate with ip: "); printIpAddress(ipAddress); logger("\n");
	if(*list == NULL) {
		// this is the first element
		IpAddressEntry* newEntry = (IpAddressEntry*)malloc(sizeof(IpAddressEntry));
		memset(newEntry, 0, sizeof(IpAddressEntry));
		memcpy(&newEntry->ipAddress, ipAddress, ipAddress->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
		memcpy(&newEntry->lastSeen, &lastSeen, sizeof(struct timeval));
		*list = newEntry;
		return;
	}
	IpAddressEntry* entry = findEntry(list, ipAddress);
	if(entry == NULL) {
		// we need to add the entry
		IpAddressEntry* lastEntry;
		for(lastEntry = *list; lastEntry->nextEntry != NULL; lastEntry = lastEntry->nextEntry);
		// we now have the last Entry in lastEntry

		// create a new Entry, copy the data and apped it to the list
		IpAddressEntry* newEntry = (IpAddressEntry*)malloc(sizeof(IpAddressEntry));
		memset(newEntry, 0, sizeof(IpAddressEntry));
		memcpy(&newEntry->ipAddress, ipAddress, ipAddress->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
		memcpy(&newEntry->lastSeen, &lastSeen, sizeof(struct timeval));
		lastEntry->nextEntry = newEntry;
	} else {
		// we only need to update the entry
		memcpy(&entry->lastSeen, &lastSeen, sizeof(struct timeval));
	}
}

void printIpAddressList(IpAddressEntry** list) {
	if(*list == NULL) {
		// the list is empty
		return;
	}
	IpAddressEntry* ipAddressIterator;
	for(ipAddressIterator = *list; ipAddressIterator != NULL; ipAddressIterator = ipAddressIterator->nextEntry) {
		// format is
		// \t[ipversion ip]
		logger("\t[");
		printIpAddressFormatted((struct sockaddr*)(&ipAddressIterator->ipAddress));
		logger("]\n");
	}
}