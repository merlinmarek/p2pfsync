#ifndef BROADCAST_H
#define BROADCAST_H

#include <sys/socket.h>

#define MESSAGE_TYPE_DISCOVER 10
#define MESSAGE_TYPE_AVAILABLE 11

typedef struct {
	char protocolId[8]; // has to be "P2PFSYNC"
	unsigned char messageType; // has to be one of the MESSAGE_TYPE_... defines
	char senderId[6]; // has to be the sender id which is obtained by getOwnId
} __attribute__((packed)) Packet; // should be packed for size and small packets

void sendIpv4Broadcasts(char senderId[6]);
void sendIpv6Multicast(char senderId[6]);
int sendAvailablePacket(struct sockaddr* destinationAddress, char senderId[6]);
int isPacketValid(Packet* packet);

// this is not used atm because the id is now sent along with the packet
// to check whether some packet is sent from oneself we now just need
// one memcmp
int isOwnAddress(struct sockaddr* address);

int isIpv4Mapped(struct sockaddr* address);
void printIpAddressFormatted(struct sockaddr* address);
void printSenderId(char buffer[6]);
void getOwnId(char buffer[6]);
int createBroadcastListener();

#endif