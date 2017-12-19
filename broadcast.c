#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>

#include "broadcast.h"
#include "defines.h"
#include "logger.h"
#include "peerList.h"
#include "shutdown.h"
#include "util.h"

#define BROADCAST_LISTENER_PORT 		44700
#define BROADCAST_LISTENER_PORT_STRING	"44700"

#define MESSAGE_TYPE_DISCOVER 10
#define MESSAGE_TYPE_AVAILABLE 11

typedef struct {
	char protocolId[8]; // has to be "P2PFSYNC"
	unsigned char messageType; // has to be one of the MESSAGE_TYPE_... defines
	char senderId[6]; // has to be the sender id which is obtained by getOwnId
} __attribute__((packed)) Packet; // should be packed for size and small packets

void sendIpv4Broadcasts(char senderId[6]);
void sendIpv6Multicast(char senderId[6]);
void getOwnId(char buffer[6]);
int sendAvailablePacket(struct sockaddr* destinationAddress, char senderId[6]);
int isPacketValid(Packet* packet);
int createBroadcastListener();

// the broadcasting thread is responsible for discovering
// new peers and responding to discovery packets sent by other peers
// this status should be in the peerList
// whenever the broadcastThread discovers a new peer it should add it to the
// peerList
void* broadcastThread(void* tid) {
	LOGD("broadcastThread started\n");

	int broadcastListener = createBroadcastListener();

	// we also need our id so other clients can match ip addresses
	char ownId[6];
	getOwnId(ownId);

	char hexBuffer[20];
	get_hex_string((unsigned char*)ownId, 6, hexBuffer, sizeof(hexBuffer));
	LOGD("own id is: %s\n", hexBuffer);

	// initialize timer here so the first broadcast is done after the first timeout
	struct timeval lastBroadcastDiscovery;
	gettimeofday(&lastBroadcastDiscovery, NULL);

	fd_set master_fds;
	FD_ZERO(&master_fds);
	FD_SET(broadcastListener, &master_fds);

	LOGD("listening to broadcast @ %d\n", broadcastListener);

	while(!getShutdown()) {
		if(get_passed_time(lastBroadcastDiscovery) > 10000.0) {
			// we want to rebroadcast after 10 seconds are over
			// this should probably be random so the chance for collisions is low?!
			// should this be something I need to think about with a layer 4 protocol like udp
			// or is this automagically done by the lower layers?
			gettimeofday(&lastBroadcastDiscovery, NULL); // reset the timer
            sendIpv6Multicast(ownId);
            sendIpv4Broadcasts(ownId);

            printPeerList();
		}

	    fd_set read_fds = master_fds;
	    struct timeval timeout;
	    memset(&timeout, 0, sizeof(timeout));
	    timeout.tv_sec = 1; // block at maximum one second at a time
	    int success = select(broadcastListener + 1, &read_fds, NULL, NULL, &timeout);
	    if(success == -1) {
	    	LOGD("select: %s\n", strerror(errno));
	    	continue;
	    }
	    if(success == 0) {
	    	// timed out
	    	continue;
	    }

	    char receiveBuffer[128];
	    struct sockaddr_storage otherAddress;
	    socklen_t otherLength = sizeof(otherAddress);
	    int receivedBytes = recvfrom(broadcastListener, receiveBuffer, sizeof(receiveBuffer)-1, 0, (struct sockaddr*)&otherAddress, &otherLength);

	    // TEST BROADCASTING WITH socat - udp-datagram:134.130.223.255:44700,broadcast
	    // or socat - upd6-sendto:[ipv6_address]:port
	    if(receivedBytes == -1) {
	    	LOGD("recvfrom failed %s\n", strerror(errno));
	    	continue;
	    }
	    if(receivedBytes == 0) {
	    	LOGD("got zero bytes :/\n");
	    	continue;
	    }

	    Packet* packet = (Packet*)receiveBuffer;
	    if(receivedBytes == sizeof(Packet) && isPacketValid(packet)) {
	    	if(memcmp(packet->senderId, ownId, 6) == 0) {
	    		// we do not want to deal with our own messages
	    		continue;
	    	}

            // when we reach this point this is a valid message from some other peer
            // check whether this is a discover request or an available message
            switch(packet->messageType) {
            case MESSAGE_TYPE_DISCOVER:
            	// we want to respond to a discovery request
            	/*logger("["); printSenderId(packet->senderId); logger("]");
            	logger("[DISCOVERY]");
            	logger("["); printIpAddressFormatted((struct sockaddr*)&otherAddress); logger("]");
            	logger("\n");
            	*/

            	sendAvailablePacket((struct sockaddr*)&otherAddress, ownId);
            	break;
            case MESSAGE_TYPE_AVAILABLE:
            	/*
            	logger("["); printSenderId(packet->senderId); logger("]");
            	logger("[AVAILABLE]");
            	logger("["); printIpAddressFormatted((struct sockaddr*)&otherAddress); logger("]");
            	logger("\n");
            	*/

            	; // needed because first statement after label

            	// we have seen the peer just now
            	struct timeval lastSeen;
            	gettimeofday(&lastSeen, NULL);
            	addIpToPeer(packet->senderId, (struct sockaddr*)&otherAddress, lastSeen);

            	/*
            	 * THIS DOES NOT BELONG HERE! WE NEED TO SOMEHOW SIGNAL TO THE COMMAND THREAD
            	 * THAT THERE IS A NEW CLIENT
            	 */

            	// send our id :)
            	//logger("trying to send\n");
            	int commandfd = socket(otherAddress.ss_family, SOCK_STREAM, 0);
            	// first we need to set the port
            	if(otherAddress.ss_family == AF_INET) {
            		// ipv4
            		struct sockaddr_in* ipv4Address = (struct sockaddr_in*)&otherAddress;
            		ipv4Address->sin_port = htons(COMMAND_LISTENER_PORT);
            	} else {
            		// ipv6
            		struct sockaddr_in6* ipv6Address = (struct sockaddr_in6*)&otherAddress;
            		ipv6Address->sin6_port = htons(COMMAND_LISTENER_PORT);
            	}
            	if(connect(commandfd, (struct sockaddr*)&otherAddress, otherLength) != 0) {
            		//logger("connect %s\n", strerror(errno));
            	}
            	const char* request = "GET /";
            	if(send(commandfd, request, strlen(request), 0) < 0) {
            		//logger("send %s\n", strerror(errno));
            	}

            	char buffer[8096];
            	int asdf = recv(commandfd, buffer, 8096, 0);
            	const char* delim = "<";
            	buffer[asdf] = 0;
            	char* entry = strtok(buffer, delim);
            	LOGD("%s\n", entry);
            	while((entry = strtok(NULL, delim)) != NULL) {
            		LOGD("%s\n", entry);
            	}

            	close(commandfd);
            	//logger("closed\n");


            	break;
            default:
            	// this should never happen
            	break;
            }
	    } else {
	    	// either we did receive a wrong amount of bytes or printPacket failed (success == 0)
	    	LOGD("received %d bytes: %*.*s\n", receivedBytes, 0, receivedBytes, receiveBuffer);
	    }
	}

	//cleanup
	close(broadcastListener);
	LOGD("broadcastThread ended\n");
	return NULL;
}


void getOwnId(char buffer[6]) {
	struct ifaddrs* interfaceList;
	int success;
	if((success = getifaddrs(&interfaceList)) != 0) {
		LOGD("getifaddrs: %s\n", gai_strerror(success));
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
			LOGD("getOwnId id generation succeded from interface: %s\n", interface->ifa_name);
			break;
		}
	}
	freeifaddrs(interfaceList);

	if(idSet == 0) {
		LOGD("getOwnId id generation failed, trying random number\n");
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
		LOGD("socket %s\n", strerror(errno));
	}

	if(setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0) {
		LOGD("setsockopt: %s\n", strerror(errno));
	}

	struct ifaddrs* interfaceList;
	int success;
	if((success = getifaddrs(&interfaceList)) != 0) {
		LOGD("getifaddrs: %s\n", gai_strerror(success));
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
            LOGD("sendto: %s\n", strerror(errno));
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
		LOGD("socket %s\n", strerror(errno));
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
		LOGD("sendto: %s\n", strerror(errno));
	}
}

int sendAvailablePacket(struct sockaddr* destinationAddress, char senderId[6]) {
	Packet packet;
	memset(&packet, 0, sizeof(Packet));
	memcpy(packet.protocolId, "P2PFSYNC", 8);
	packet.messageType = MESSAGE_TYPE_AVAILABLE;
	memcpy(packet.senderId, senderId, 6);

	int senderSocket = socket(destinationAddress->sa_family, SOCK_DGRAM, 0);
	if(senderSocket == -1) {
		LOGD("socket: %s\n", strerror(errno));
		return -1;
	}
    if(destinationAddress->sa_family == AF_INET6) {
        ((struct sockaddr_in6*)destinationAddress)->sin6_port = htons(BROADCAST_LISTENER_PORT);
    } else if(destinationAddress->sa_family == AF_INET) {
        ((struct sockaddr_in*)destinationAddress)->sin_port = htons(BROADCAST_LISTENER_PORT);
    }

	socklen_t destinationAddressSize = destinationAddress->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
	if(sendto(senderSocket, &packet, sizeof(packet), 0, destinationAddress, destinationAddressSize) == -1) {
		LOGD("sendto: %s\n", strerror(errno));
		return -1;
	}
	//logger("sent an available packet\n");
	return 0;
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

// we cannot use the socketUtil functions for this because in case we get
// an ipv6 socket we have to join the multicast group as well
int createBroadcastListener() {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* resultList;
	int success = getaddrinfo(NULL, BROADCAST_LISTENER_PORT_STRING, &hints, &resultList);
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
		// try to join the ipv6 multicast group
		if(bind(listenerSocket, iterator->ai_addr, iterator->ai_addrlen) == 0) {
            // join membership
            struct ipv6_mreq group;
            group.ipv6mr_interface = 0;
            inet_pton(AF_INET6, IPV6_MULTICAST_ADDRESS, &group.ipv6mr_multiaddr);
            if(setsockopt(listenerSocket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &group, sizeof(group)) == -1) {
            	LOGD("setsockopt %s\n", strerror(errno));
            }
			break;
		}
		close(listenerSocket);
		listenerSocket = -1;
	}
	if(listenerSocket == -1) {
		LOGD("no ipv6 listener could be created, trying for ipv4 instead\n");
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
