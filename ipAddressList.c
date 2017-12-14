#include "ipAddressList.h"
#include "logger.h"

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
		if(((struct sockaddr*)ipAddressIterator->ipAddress)->sa_family == ipAddress->sa_family) {
			return ipAddressIterator;
		}
	}
	return NULL;
}

void createIpAddressList(IpAddressEntry** list) {
	// the creation should also be handled by addOrUpdate
	// if an empty list is provided that function should create the list
	/*
	if(*list != NULL) {
		// if we are given a not empty list we want to free it first
		// so we do not create any memory leaks
		freeIpAddressList(list);
	}
	*list = (IpAddressEntry*)malloc(sizeof(IpAddressEntry));
	// maybe there should be a check if malloc failed...
	memset(*list, 0, sizeof(IpAddressEntry));
	*/
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
	if(*list == NULL) {
		// this is the first element

		return;
	}
	IpAddressEntry* entry = findEntry(list, ipAddress);
	if(entry == NULL) {
		// we need to add the entry
		IpAddressEntry* lastEntry;
		for(lastEntry = *list; lastEntry != NULL; lastEntry = lastEntry->nextEntry);
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
	IpAddressEntry* ipAddressIterator;
	for(ipAddressIterator = *list; ipAddressIterator != NULL; ipAddressIterator = ipAddressIterator->nextEntry) {
		// print the ip address along the last seen date here
	}
}
