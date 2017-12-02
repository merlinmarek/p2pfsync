#ifndef BROADCAST_H
#define BROADCAST_H

#include <sys/socket.h>

#define MESSAGE_TYPE_DISCOVER 10
#define MESSAGE_TYPE_AVAILABLE 11


typedef struct {
	char identifier[8]; // has to be "P2PFSYNC"
	unsigned char messageType;
	// the ip fields are set to the sending ip address
	unsigned char ipVersion; // has to be one of { IPV4, IPV6 }
	union {
        char ipv4address[4];
        char ipv6address[16];
	};
} __attribute__((packed)) Packet; // should be packed for size and small packets

int createBroadcastListener(const char* port); // broadcast listeners are always UDP
int sendBroadcastPacket(struct sockaddr* sourceAddress, struct sockaddr* destinationAddress);
int sendAvailablePacket(struct sockaddr* sourceAddress, struct sockaddr* destinationAddress);
int printPacket(Packet* packet);
int isPacketValid(Packet* packet);
int isOwnAddress(unsigned char ipVersion, const char* ipAddress);
void sendBroadcastDiscover();

#endif
