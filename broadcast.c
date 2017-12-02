#include <errno.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "defines.h"
#include "logger.h"
#include "broadcast.h"


int sendBroadcastPacket(struct sockaddr* sourceAddress, struct sockaddr* destinationAddress) {
	Packet broadcastPacket;
	memset(&broadcastPacket, 0, sizeof(Packet));
	memcpy(broadcastPacket.identifier, "P2PFSYNC", 8);
	broadcastPacket.messageType = MESSAGE_TYPE_DISCOVER;
	broadcastPacket.ipVersion = sourceAddress->sa_family == AF_INET ? IPV4 : IPV6;
	if(sourceAddress->sa_family == AF_INET) {
		memcpy(broadcastPacket.ipv4address, &(((struct sockaddr_in*)sourceAddress)->sin_addr.s_addr), 4);
	} else {
		memcpy(broadcastPacket.ipv6address, &(((struct sockaddr_in6*)sourceAddress)->sin6_addr.__in6_u), 16);
	}
	// udp socket creation is cheap enough that we can create a new socket for each broadcast packet
	int senderSocket = socket(destinationAddress->sa_family, SOCK_DGRAM, 0);
	if(senderSocket == -1) {
		logger("socket: %s\n", strerror(errno));
		return -1;
	}
	if(setsockopt(senderSocket, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0) {
		logger("setsockopt(SOBROADCAST): %s\n", strerror(errno));
		return -1;
	}
	socklen_t destinationAddressSize = destinationAddress->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
	if(sendto(senderSocket, &broadcastPacket, sizeof(broadcastPacket), 0, destinationAddress, destinationAddressSize) == -1) {
		logger("sendto: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int sendAvailablePacket(struct sockaddr* sourceAddress, struct sockaddr* destinationAddress) {
	Packet availablePacket;
	memset(&availablePacket, 0, sizeof(Packet));
	memcpy(availablePacket.identifier, "P2PFSYNC", 8);
	availablePacket.messageType = MESSAGE_TYPE_AVAILABLE;
	availablePacket.ipVersion = sourceAddress->sa_family == AF_INET ? IPV4 : IPV6;
	if(sourceAddress->sa_family == AF_INET) {
		memcpy(availablePacket.ipv4address, &(((struct sockaddr_in*)sourceAddress)->sin_addr.s_addr), 4);
	} else {
		memcpy(availablePacket.ipv6address, &(((struct sockaddr_in6*)sourceAddress)->sin6_addr.__in6_u), 16);
	}
	// udp socket creation is cheap enough that we can create a new socket for each broadcast packet
	int senderSocket = socket(destinationAddress->sa_family, SOCK_DGRAM, 0);
	if(senderSocket == -1) {
		logger("socket: %s\n", strerror(errno));
		return -1;
	}
	socklen_t destinationAddressSize = destinationAddress->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
	if(sendto(senderSocket, &availablePacket, sizeof(availablePacket), 0, destinationAddress, destinationAddressSize) == -1) {
		logger("sendto: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

int isOwnAddress(unsigned char ipVersion, const char* ipAddress) {
	int isOwn = 0; // init with false
	struct ifaddrs* interfaceList;
	if(getifaddrs(&interfaceList) != 0) {
		logger("getifaddrs");
	}

	// now we can iterate over the devices
	struct ifaddrs* interface;
	for(interface = interfaceList; interface != NULL; interface = interface->ifa_next) {
		if(interface->ifa_addr->sa_family == (ipVersion == IPV4 ? AF_INET : AF_INET6)) {
			if(interface->ifa_addr->sa_family == AF_INET) {
				struct sockaddr_in* interfaceIpv4 = (struct sockaddr_in*)(interface->ifa_addr);
				if(memcmp(&(interfaceIpv4->sin_addr.s_addr), ipAddress, 4) == 0) {
					isOwn = 1;
				}
			} else if(interface->ifa_addr->sa_family == AF_INET6) {
				struct sockaddr_in6* interfaceIpv6 = (struct sockaddr_in6*)(interface->ifa_addr);
				if(memcmp(&(interfaceIpv6->sin6_addr.__in6_u), ipAddress, 16) == 0) {
					isOwn = 1;
				}
			}
		}
	}
	freeifaddrs(interfaceList);
	return isOwn;
}

int isPacketValid(Packet* packet) {
	if(memcmp(packet->identifier, "P2PFSYNC", 8) != 0) {
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
	if(packet->ipVersion != IPV4 && packet->ipVersion != IPV6) {
		return 0;
	}
	return 1;
}

int printPacket(Packet* packet) {
	if(memcmp(packet->identifier, "P2PFSYNC", 8) != 0) {
		logger("invalid identifier: %*.*s\n", 0, 8, packet->identifier);
		return -1;
	}
	switch((unsigned int)(packet->messageType)) {
	case MESSAGE_TYPE_DISCOVER:
		break;
	case MESSAGE_TYPE_AVAILABLE:
		break;
	default:
		logger("invalid message type: %u\n", (unsigned int)(packet->messageType));
		return -1;
		break;
	}
	if(packet->ipVersion != IPV4 && packet->ipVersion != IPV6) {
		logger("invalid ip version: %u\n", packet->ipVersion);
		return -1;
	}
	char ipBuffer[256];
	memset(ipBuffer, 0, sizeof(ipBuffer));
	inet_ntop(packet->ipVersion == IPV4 ? AF_INET : AF_INET6, packet->ipv6address, ipBuffer, sizeof(ipBuffer));
	logger("got broadcast packet from: %s which is %s ip\n", ipBuffer, isOwnAddress(packet->ipVersion, packet->ipv4address) ? "my own" : "a remote");
	return 0;
}

void sendBroadcastDiscover() {
	struct ifaddrs* interfaceList;
	if(getifaddrs(&interfaceList) != 0) {
		logger("getifaddrs");
	}

	// now we can iterate over the devices
	struct ifaddrs* interface;
	for(interface = interfaceList; interface != NULL; interface = interface->ifa_next) {
		if(interface->ifa_addr == NULL || interface->ifa_addr->sa_family != AF_INET) {
			// we skip interfaces that lack an ip or have no IPv4 address
			// it dont know how to multicast to ipv6 so we only list ipv4 for now
			continue;
		}
		struct in_addr ipv4Address = ((struct sockaddr_in*)interface->ifa_addr)->sin_addr;
		struct in_addr ipv4Netmask = ((struct sockaddr_in*)interface->ifa_netmask)->sin_addr;
		struct in_addr ipv4Broadcast;
		memset(&ipv4Broadcast, 0, sizeof(ipv4Broadcast));
		ipv4Broadcast.s_addr = ipv4Address.s_addr | (~ipv4Netmask.s_addr);

		// now we want to send a hello message to broadcast
		struct sockaddr_in destinationAddress;
		memset(&destinationAddress, 0, sizeof(destinationAddress));
		destinationAddress.sin_addr = ipv4Broadcast;
		destinationAddress.sin_family = AF_INET;
		destinationAddress.sin_port = htons(BROADCAST_LISTENER_PORT);

		if(sendBroadcastPacket(interface->ifa_addr, (struct sockaddr*)&destinationAddress) == 0) {
			// 0 = success
			logger("sent broadcast packet to: %s\t[%s]\n", inet_ntoa(destinationAddress.sin_addr), interface->ifa_name);
		}
	}
	freeifaddrs(interfaceList);
}
