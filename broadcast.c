#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <time.h>
#include <linux/if_packet.h>

#include "defines.h"
#include "logger.h"
#include "broadcast.h"

void printSenderId(char buffer[6]) {
	logger("%02x%02x%02x%02x%02x%02x",
			(unsigned char)buffer[0],
			(unsigned char)buffer[1],
			(unsigned char)buffer[2],
			(unsigned char)buffer[3],
			(unsigned char)buffer[4],
			(unsigned char)buffer[5]);
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

void getOwnId(char buffer[6]) {
	struct ifaddrs* interfaceList;
	int success;
	if((success = getifaddrs(&interfaceList)) != 0) {
		logger("getifaddrs: %s\n", gai_strerror(success));
		return;
	}

	int idSet = 0;

	// now we can iterate over the interfaces
	struct ifaddrs* interface;
	for(interface = interfaceList; interface != NULL; interface = interface->ifa_next) {
		if(interface->ifa_addr->sa_family == AF_PACKET && strcmp(interface->ifa_name, "lo") != 0) {
			// mac address is in AF_PACKET interfaces
			// we also need to skip the loopback device because its mac is always just zeros
			struct sockaddr_ll* address = (struct sockaddr_ll*)interface->ifa_addr;
			memcpy(buffer, address->sll_addr, 6);
			idSet = 1;
			logger("getOwnId id generation succeded from interface: %s\n", interface->ifa_name);
			break;
		}
	}
	freeifaddrs(interfaceList);

	if(idSet == 0) {
		logger("getOwnId id generation failed, trying random number\n");
		// somehow the id generation failed :/ so we generate a random number instead
		// we need to seed the random generator here so not everyone generates the same
		srand(time(0));
		int i;
		for(i = 0; i < 6; ++i) {
			buffer[i] = rand() & 0xFF;
		}
	}
}

// the broadcast has to be sent for every device
void sendIpv4Broadcasts(char senderId[6]) {
	int broadcastSocket = socket(AF_INET6, SOCK_DGRAM, 0);
	if(broadcastSocket == -1) {
		logger("socket %s\n", strerror(errno));
	}

	if(setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0) {
		logger("setsockopt: %s\n", strerror(errno));
	}

	struct ifaddrs* interfaceList;
	int success;
	if((success = getifaddrs(&interfaceList)) != 0) {
		logger("getifaddrs: %s\n", gai_strerror(success));
		return;
	}

	// now we can iterate over the interfaces
	struct ifaddrs* interface;
	for(interface = interfaceList; interface != NULL; interface = interface->ifa_next) {
		if(interface->ifa_addr == NULL || interface->ifa_addr->sa_family != AF_INET) {
			// only consider interfaces that have a valid ipv4 address
			continue;
		}

		// now we have to calculate the broadcast address for the interface
        struct sockaddr_in ipv4Address;
        memset(&ipv4Address, 0, sizeof(ipv4Address));
        ipv4Address.sin_addr.s_addr = ((struct sockaddr_in*)interface->ifa_addr)->sin_addr.s_addr | (~((struct sockaddr_in*)interface->ifa_netmask)->sin_addr.s_addr);
        ipv4Address.sin_family = AF_INET;
        ipv4Address.sin_port = htons(BROADCAST_LISTENER_PORT);

        Packet packet;
        memcpy(packet.protocolId, "P2PFSYNC", 8);
        packet.messageType = MESSAGE_TYPE_DISCOVER;
        memcpy(packet.senderId, senderId, 6);
        if(sendto(broadcastSocket, &packet, sizeof(packet), 0, (struct sockaddr*)&ipv4Address, sizeof(ipv4Address)) == -1) {
            logger("sendto: %s\n", strerror(errno));
        }
	}
	freeifaddrs(interfaceList);
}

// the multicast is sent once to the multicast address
// I am not sure atm if this needs to be done per interface
// for ipv4 it is easy as every interface has a different
// ipv4 broadcast address. The multicast address for ipv6
// is unique as far as i know
// there are actually ways to set the interface for sending
// ipv6 multicasts but it is not trivial. Idk if it is even possible
// without root priviliges so we will stick with a single multicast here
void sendIpv6Multicast(char senderId[6]) {
	int multicastSocket = socket(AF_INET6, SOCK_DGRAM, 0);
	if(multicastSocket == -1) {
		logger("socket %s\n", strerror(errno));
	}

	struct sockaddr_in6 ipv6Address;
	memset(&ipv6Address, 0, sizeof(ipv6Address));
	ipv6Address.sin6_family = AF_INET6;
	ipv6Address.sin6_port = htons(BROADCAST_LISTENER_PORT);
	inet_pton(AF_INET6, IPV6_MULTICAST_ADDRESS, &ipv6Address.sin6_addr);

	Packet packet;
    memcpy(packet.protocolId, "P2PFSYNC", 8);
	packet.messageType = MESSAGE_TYPE_DISCOVER;
	memcpy(packet.senderId, senderId, 6);
	if(sendto(multicastSocket, &packet, sizeof(packet), 0, (struct sockaddr*)&ipv6Address, sizeof(ipv6Address)) == -1) {
		logger("sendto: %s\n", strerror(errno));
	}
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

int sendAvailablePacket(struct sockaddr* destinationAddress, char senderId[6]) {
	Packet packet;
	memset(&packet, 0, sizeof(Packet));
	memcpy(packet.protocolId, "P2PFSYNC", 8);
	packet.messageType = MESSAGE_TYPE_AVAILABLE;
	memcpy(packet.senderId, senderId, 6);

	int senderSocket = socket(destinationAddress->sa_family, SOCK_DGRAM, 0);
	if(senderSocket == -1) {
		logger("socket: %s\n", strerror(errno));
		return -1;
	}
    if(destinationAddress->sa_family == AF_INET6) {
        ((struct sockaddr_in6*)destinationAddress)->sin6_port = htons(BROADCAST_LISTENER_PORT);
    } else if(destinationAddress->sa_family == AF_INET) {
        ((struct sockaddr_in*)destinationAddress)->sin_port = htons(BROADCAST_LISTENER_PORT);
    }

	socklen_t destinationAddressSize = destinationAddress->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
	if(sendto(senderSocket, &packet, sizeof(packet), 0, destinationAddress, destinationAddressSize) == -1) {
		logger("sendto: %s\n", strerror(errno));
		return -1;
	}
	//logger("sent an available packet\n");
	return 0;
}

int isOwnAddress(struct sockaddr* address) {
	int isOwn = 0; // init with false
	struct ifaddrs* interfaceList;
	if(getifaddrs(&interfaceList) != 0) {
		logger("getifaddrs");
	}

	// now we can iterate over the devices and check if any of the devices has this ip address
	struct ifaddrs* interface;
	for(interface = interfaceList; interface != NULL; interface = interface->ifa_next) {
		if(interface->ifa_addr->sa_family == address->sa_family) {
			if(interface->ifa_addr->sa_family == AF_INET) {
				struct sockaddr_in* interfaceIpv4 = (struct sockaddr_in*)(interface->ifa_addr);
				struct sockaddr_in* givenIpv4 = (struct sockaddr_in*)(address);
				if(memcmp(&(interfaceIpv4->sin_addr.s_addr), &(givenIpv4->sin_addr.s_addr), 4) == 0) {
					isOwn = 1;
				}
			} else if(interface->ifa_addr->sa_family == AF_INET6) {
				struct sockaddr_in6* interfaceIpv6 = (struct sockaddr_in6*)(interface->ifa_addr);
				struct sockaddr_in6* givenIpv6 = (struct sockaddr_in6*)(address);
				if(memcmp(&(interfaceIpv6->sin6_addr.__in6_u), &(givenIpv6->sin6_addr.__in6_u), 16) == 0) {
					isOwn = 1;
				}
			}
		} else if(interface->ifa_addr->sa_family == AF_INET && isIpv4Mapped(address)) {
			// check if it is an ipv6 mapped ipv4 address
			struct sockaddr_in* interfaceIpv4 = (struct sockaddr_in*)(interface->ifa_addr);
			struct sockaddr_in6* givenIpv6 = (struct sockaddr_in6*)(address);
			if(memcmp(&(interfaceIpv4->sin_addr.s_addr), ((unsigned short*)(&givenIpv6->sin6_addr.__in6_u)) + 6, 4) == 0) {
				isOwn = 1;
			}
		}
	}
	freeifaddrs(interfaceList);
	return isOwn;
}

int isPacketValid(Packet* packet) {
	if(memcmp(packet->protocolId, "P2PFSYNC", 8) != 0) {
		return 0;
	}
	switch((unsigned int)(packet->messageType)) {
	case MESSAGE_TYPE_DISCOVER:
		break;
	case MESSAGE_TYPE_AVAILABLE:
		break;
	default:
		return 0;
		break;
	}
	return 1;
}

int createBroadcastListener() {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* resultList;
	int success = getaddrinfo(NULL, BROADCAST_LISTENER_PORT_STRING, &hints, &resultList);
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
		// try to join the ipv6 multicast group
		if(bind(listenerSocket, iterator->ai_addr, iterator->ai_addrlen) == 0) {
            // join membership
            struct ipv6_mreq group;
            group.ipv6mr_interface = 0;
            inet_pton(AF_INET6, IPV6_MULTICAST_ADDRESS, &group.ipv6mr_multiaddr);
            if(setsockopt(listenerSocket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &group, sizeof(group)) == -1) {
            	logger("setsockopt %s\n", strerror(errno));
            }
			break;
		}
		close(listenerSocket);
		listenerSocket = -1;
	}
	if(listenerSocket == -1) {
		logger("no ipv6 listener could be created, trying for ipv4 instead\n");
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
