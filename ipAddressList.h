#ifndef IP_ADDRESS_LIST_H
#define IP_ADDRESS_LIST_H

#include <sys/time.h>
#include <sys/socket.h>

typedef struct ipAddressEntry {
	struct ipAddressEntry* nextEntry;
	struct sockaddr_storage ipAddress;
	struct timeval lastSeen;
} IpAddressEntry;

void createIpAddressList(IpAddressEntry** list);
void freeIpAddressList(IpAddressEntry** list);
void addOrUpdateEntry(IpAddressEntry** list, struct sockaddr* ipAddress, struct timeval lastSeen);
// passing the ip address by ** is not really necessary for printIpAddressList but it is more
// convenient if list is always **
void printIpAddressList(IpAddressEntry** list);

#endif
