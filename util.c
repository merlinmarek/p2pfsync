#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "logger.h"
#include "broadcast.h"
#include "util.h"

void get_hex_string(const unsigned char* in_buffer, const size_t count, char* buffer, const size_t buffer_size) {
	size_t i;
	for(i = 0; i < count && 2 * (i + 1) < buffer_size; i++) {
		sprintf(&buffer[2*i], "%02x", in_buffer[i]);
	}
	buffer[2 * i] = 0;
}

// gives the difference t1 - t2 in milliseconds
double get_time_difference_ms(struct timeval t1, struct timeval t2) {
    double elapsedTime = (t1.tv_sec - t2.tv_sec) * 1000.0; // sec to ms
    elapsedTime += (t1.tv_usec - t2.tv_usec) / 1000.0;		// us to ms
    return elapsedTime;
}

// gives the difference now - t
double get_passed_time(struct timeval t) {
	struct timeval now;
	gettimeofday(&now, 0);
	return get_time_difference_ms(now, t);
}

void get_ip_address_formatted(struct sockaddr* address, char* buffer, size_t buffer_size) {
	size_t index = 0;
	if(address->sa_family == AF_INET) {
		// ipv4 address
		strncpy(&buffer[index], "IPv4 ", 5);
		index += 5;
		struct sockaddr_in* ipv4Address = (struct sockaddr_in*)address;
		inet_ntop(address->sa_family, &(ipv4Address->sin_addr), &buffer[index], buffer_size - index);
	} else if(address->sa_family == AF_INET6) {
		if(is_ipv4_mapped(address)) {
			// this is an ipv6 mapped ipv4 address
			// the ipv4 address is stored in the last 4 bytes of the 16 byte ipv6 address
            strncpy(&buffer[index], "IPv4 ", 5);
            index += 5;
			struct sockaddr_in6* ipv6Address = (struct sockaddr_in6*)address;
			char* ipv4AddressStart = ((char*)(&ipv6Address->sin6_addr.__in6_u)) + 12;
			struct in_addr* ipv4InAddr = (struct in_addr*)ipv4AddressStart;
			inet_ntop(AF_INET, ipv4InAddr, &buffer[index], buffer_size - index);
		} else {
			// regular ipv6 address
            strncpy(&buffer[index], "IPv6 ", 5);
            index += 5;
			struct sockaddr_in6* ipv6Address = (struct sockaddr_in6*)address;
			inet_ntop(AF_INET6, &(ipv6Address->sin6_addr), &buffer[index], buffer_size - index);
		}
	} else {
		strncpy(&buffer[index], "unkown address family", buffer_size - index);
	}
	// make sure that the buffer is 0 terminated
	buffer[buffer_size - 1] = 0;
}

int create_listener_socket(const char* port, int ai_socktype) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = ai_socktype;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* resultList;
	int success = getaddrinfo(NULL, port, &hints, &resultList);
	if(success != 0) {
		LOGD("getaddrinfo: %s\n", gai_strerror(success));
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
			LOGD("socket: %s\n", strerror(errno));
			continue;
		}
		if (setsockopt(listenerSocket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
			LOGD("setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
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
				LOGD("socket: %s\n", strerror(errno));
				continue;
			}
			if (setsockopt(listenerSocket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
				LOGD("setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
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

int create_udp_listener(const char* port) {
	return create_listener_socket(port, SOCK_DGRAM);
}

int create_tcp_listener(const char* port) {
	return create_listener_socket(port, SOCK_STREAM);
}

// returns whether some ipv6 address is actually an ipv6 mapped
// ipv4 address
int is_ipv4_mapped(struct sockaddr* address) {
	if(address->sa_family != AF_INET6) {
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
