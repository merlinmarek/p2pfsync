#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "logger.h"
#include "socketUtil.h"
#include "broadcast.h"

void printIpAddress(const struct sockaddr* address) {
	char ipBuffer[256];
	memset(ipBuffer, 0, sizeof(ipBuffer));
	if(address->sa_family == AF_INET) {
		inet_ntop(address->sa_family, &(((struct sockaddr_in*)address)->sin_addr), ipBuffer, sizeof(ipBuffer));
	} else if(address->sa_family == AF_INET6) {
		inet_ntop(address->sa_family, &(((struct sockaddr_in6*)address)->sin6_addr), ipBuffer, sizeof(ipBuffer));
	} else {
		logger("unkown address family %d", address->sa_family);
		return;
	}
	logger("%s", ipBuffer);
}

void printIpAddressFormatted(struct sockaddr* address) {
	char ipBuffer[256];
	memset(ipBuffer, 0, sizeof(ipBuffer));
	if(address->sa_family == AF_INET) {
		// ipv4 address
		logger("IPv4 ");
		struct sockaddr_in* ipv4Address = (struct sockaddr_in*)address;
		inet_ntop(address->sa_family, &(ipv4Address->sin_addr), ipBuffer, sizeof(ipBuffer));
	} else if(address->sa_family == AF_INET6) {
		if(isIpv4Mapped(address)) {
			// this is an ipv6 mapped ipv4 address
			// the ipv4 address is stored in the last 4 bytes of the 16 byte ipv6 address
			logger("IPv4 ");
			struct sockaddr_in6* ipv6Address = (struct sockaddr_in6*)address;
			char* ipv4AddressStart = ((char*)(&ipv6Address->sin6_addr.__in6_u)) + 12;
			struct in_addr* ipv4InAddr = (struct in_addr*)ipv4AddressStart;
			inet_ntop(AF_INET, ipv4InAddr, ipBuffer, sizeof(ipBuffer));
		} else {
			// regular ipv6 address
			logger("IPv6 ");
			struct sockaddr_in6* ipv6Address = (struct sockaddr_in6*)address;
			inet_ntop(AF_INET6, &(ipv6Address->sin6_addr), ipBuffer, sizeof(ipBuffer));
		}

	} else {
		logger("unkown address family %d", address->sa_family);
		return;
	}
	logger("%s", ipBuffer);
}

int createListenerSocket(const char* port, int ai_socktype) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = ai_socktype;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* resultList;
	int success = getaddrinfo(NULL, port, &hints, &resultList);
	if(success != 0) {
		logger("getaddrinfo: %s\n", gai_strerror(success));
		return -1;
	}

	int listenerSocket = -1;
	struct addrinfo* iterator;

	for(iterator = resultList; iterator != NULL; iterator = iterator->ai_next) {
		// first try to get IPv6 address
		if(iterator->ai_family != AF_INET6) {
			continue;
		}
		listenerSocket = socket(iterator->ai_family, iterator->ai_socktype, iterator->ai_protocol);
		if(listenerSocket == -1) {
			logger("socket: %s\n", strerror(errno));
			continue;
		}
		if (setsockopt(listenerSocket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
			logger("setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
			continue;
		}
		if(bind(listenerSocket, iterator->ai_addr, iterator->ai_addrlen) == 0) {
			break;
		}
		close(listenerSocket);
		listenerSocket = -1;
	}
	if(listenerSocket == -1) {
		// try to get ipv4 instead
		for(iterator = resultList; iterator != NULL; iterator = iterator->ai_next) {
			if(iterator->ai_family != AF_INET) {
				continue;
			}
			listenerSocket = socket(iterator->ai_family, iterator->ai_socktype, iterator->ai_protocol);
			if(listenerSocket == -1) {
				logger("socket: %s\n", strerror(errno));
				continue;
			}
			if (setsockopt(listenerSocket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
				logger("setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
			}
			if(bind(listenerSocket, iterator->ai_addr, iterator->ai_addrlen) == 0) {
				break;
			}
			close(listenerSocket);
			listenerSocket = -1;
		}
	}
	return listenerSocket;
}

int createUDPListener(const char* port) {
	return createListenerSocket(port, SOCK_DGRAM);
}

int createTCPListener(const char* port) {
	return createListenerSocket(port, SOCK_STREAM);
}

// returns whether some ipv6 address is actually an ipv6 mapped
// ipv4 address
int isIpv4Mapped(struct sockaddr* address) {
	if(address->sa_family != AF_INET6) {
		logger("address not in ipv6 format, no mapping possible\n");
		return 0;
	}
	struct sockaddr_in6* ipv6Address = (struct sockaddr_in6*)address;
	unsigned short* ipv6AddressWords = (unsigned short*)(&ipv6Address->sin6_addr);
	if(ipv6AddressWords[0] == 0 &&
		ipv6AddressWords[1] == 0 &&
		ipv6AddressWords[2] == 0 &&
		ipv6AddressWords[3] == 0 &&
		ipv6AddressWords[4] == 0 &&
		ipv6AddressWords[5] == 0xffff) {
		return 1;
	}
	return 0;
}
