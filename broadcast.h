#ifndef BROADCAST_H
#define BROADCAST_H

#include <sys/socket.h>

#define MESSAGE_TYPE_DISCOVER 10
#define MESSAGE_TYPE_AVAILABLE 11


typedef struct {
	char identifier[8]; // has to be "P2PFSYNC"
	unsigned char messageType;
} __attribute__((packed)) Packet; // should be packed for size and small packets

void sendIpv4Broadcasts();
void sendIpv6Multicast();
int sendAvailablePacket(struct sockaddr* destinationAddress);
int isPacketValid(Packet* packet);
int isOwnAddress(struct sockaddr* address);
int isIpv4Mapped(struct sockaddr* address);
void sendBroadcastDiscover();
int createBroadcastListener();

#endif
