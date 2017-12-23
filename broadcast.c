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
#include "command_client.h"
#include "command_server.h"
#include "file_client.h"
#include "file_server.h"
#include "defines.h"
#include "logger.h"
#include "message_queue.h"
#include "peer_list.h"
#include "shutdown.h"
#include "util.h"

#define BROADCAST_LISTENER_PORT 		44700
#define BROADCAST_LISTENER_PORT_STRING	"44700"

#define MESSAGE_TYPE_DISCOVER 10
#define MESSAGE_TYPE_AVAILABLE 11

typedef struct {
	char protocol_id[8]; // has to be "P2PFSYNC"
	unsigned char message_type; // has to be one of the MESSAGE_TYPE_... defines
	char sender_id[6]; // has to be the sender id which is obtained by getOwnId
} __attribute__((packed)) packet_type; // should be packed for size and consistency across nodes

int create_broadcast_listener();
int is_packet_valid(packet_type* packet);
int send_available_packet(struct sockaddr* destination_address, char sender_id[6]);
void send_ipv4_broadcast(char sender_id[6]);
void send_ipv6_multicast(char sender_id[6]);
void get_own_id(char buffer[6]);

static message_queue_type* message_queue = NULL;

void broadcast_thread_send_message(message_queue_entry_type* message) {
	message_queue_push(message_queue, message);
}

void peer_seen(char* peer_id, struct sockaddr* peer_ip) {
	// THIS FUNCTIONS NEEDS SOME CHECKING WHETHER SOME PEER ALREADY EXISTS
	if(!is_peer_in_list(peer_id)) {
        message_data_peer_seen_type peer_seen_data;
        memset(&peer_seen_data, 0, sizeof(peer_seen_data));
        memcpy(peer_seen_data.peer_id, peer_id, 6);
        gettimeofday(&peer_seen_data.timestamp, NULL);
        memcpy(&peer_seen_data.address, peer_ip, peer_ip->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
        message_queue_entry_type* message = message_queue_create_message("peer_seen", &peer_seen_data, sizeof(peer_seen_data));
        command_client_thread_send_message(message);
	}
    struct timeval now;
    gettimeofday(&now, NULL);
    add_ip_to_peer(peer_id, peer_ip, now);
}

// the broadcasting thread is responsible for discovering
// new peers and responding to discovery packets sent by other peers
// this status should be in the peerList
// whenever the broadcastThread discovers a new peer it should add it to the
// peerList
void* broadcast_thread(void* tid) {
	LOGD("started\n");

	// this has to be called otherwise this thread will not be able to receive any messages
	message_queue = message_queue_create_queue();

	int broadcast_listener = create_broadcast_listener();

	// we also need our id so other clients can match ip addresses
	char own_id[6];
	get_own_id(own_id);

	char hex_buffer[20];
	get_hex_string((unsigned char*)own_id, 6, hex_buffer, sizeof(hex_buffer));
	LOGD("own id is: %s\n", hex_buffer);

	// initialize timer here so the first broadcast is done after the first timeout
	struct timeval last_broadcast_discovery;
	gettimeofday(&last_broadcast_discovery, NULL);

	fd_set master_set;
	FD_ZERO(&master_set);
	FD_SET(broadcast_listener, &master_set);

	LOGD("listening to broadcast @ %d\n", broadcast_listener);

	while(!get_shutdown()) {
		// handle messages sent by other threads
		message_queue_entry_type* message;
		while((message = message_queue_pop(message_queue)) != NULL) {
			LOGD("received message: %s\n", message->message_id);
			message_queue_free_message(message);
		}

		if(get_passed_time(last_broadcast_discovery) > 10000.0) {
			// we want to rebroadcast after 10 seconds are over
			// this should probably be random so the chance for collisions is low?!
			// should this be something I need to think about with a layer 4 protocol like udp
			// or is this automagically done by the lower layers?
			gettimeofday(&last_broadcast_discovery, NULL); // reset the timer
            //send_ipv6_multicast(own_id);
            send_ipv4_broadcast(own_id);
            //LOGD("sending broadcast discovery\n");

            struct sockaddr_in ip;
            ip.sin_family = AF_INET;
            ip.sin_port = htons(12345);
            ip.sin_addr.s_addr = inet_addr("127.0.0.1");

            // this is only for testing purposes
            //peer_seen("abcdef", (struct sockaddr*)&ip);

            //print_peer_list();
		}

	    fd_set read_set = master_set;
	    struct timeval timeout;
	    memset(&timeout, 0, sizeof(timeout));
	    timeout.tv_sec = 1; // block at maximum one second at a time
	    int success = select(broadcast_listener + 1, &read_set, NULL, NULL, &timeout);
	    if(success == -1) {
	    	LOGD("select: %s\n", strerror(errno));
	    	continue;
	    }
	    if(success == 0) {
	    	// timed out
	    	continue;
	    }

	    char receive_buffer[128];
	    struct sockaddr_storage other_address;
	    socklen_t other_length = sizeof(other_address);
	    int received_bytes = recvfrom(broadcast_listener, receive_buffer, sizeof(receive_buffer)-1, 0, (struct sockaddr*)&other_address, &other_length);

	    // TEST BROADCASTING WITH socat - udp-datagram:134.130.223.255:44700,broadcast
	    // or socat - upd6-sendto:[ipv6_address]:port
	    if(received_bytes == -1) {
	    	LOGD("recvfrom failed %s\n", strerror(errno));
	    	continue;
	    }
	    if(received_bytes == 0) {
	    	LOGD("got zero bytes\n");
	    	continue;
	    }

	    packet_type* packet = (packet_type*)receive_buffer;
	    if(received_bytes == sizeof(packet_type) && is_packet_valid(packet)) {
	    	if(memcmp(packet->sender_id, own_id, 6) == 0) {
	    		// we do not want to deal with our own messages
	    		continue;
	    	}

            // when we reach this point this is a valid message from some other peer
            // check whether this is a discover request or an available message
            switch(packet->message_type) {
            case MESSAGE_TYPE_DISCOVER:
            	// here should be some peerSeen() function that can also be called by the MESSAGE_TYPE_AVAILABLE case
            	peer_seen(packet->sender_id, (struct sockaddr*)&other_address);
            	send_available_packet((struct sockaddr*)&other_address, own_id);
            	break;
            case MESSAGE_TYPE_AVAILABLE:
                // THOUGHTS
                // the broadcast thread does not need to know about the peers, only the command_thread does
            	// the broadcast thread just sends the command_thread a message whenever a new peer is discovered
            	// or seen again. the command thread then handles it all.
            	peer_seen(packet->sender_id, (struct sockaddr*)&other_address);
            	break;
            default:
            	// this should never happen
            	break;
            }
	    } else {
	    	// either we did receive a wrong amount of bytes or printPacket failed (success == 0)
	    	LOGD("received %d bytes: %*.*s\n", received_bytes, 0, received_bytes, receive_buffer);
	    }
	}

	//cleanup
	close(broadcast_listener);
	message_queue_free_queue(message_queue);
	message_queue = NULL;

	LOGD("ended\n");
	return NULL;
}


void get_own_id(char buffer[6]) {
	struct ifaddrs* interface_list;
	int success;
	if((success = getifaddrs(&interface_list)) != 0) {
		LOGD("getifaddrs: %s\n", gai_strerror(success));
		return;
	}

	int id_set = 0;

	// now we can iterate over the interfaces
	struct ifaddrs* interface;
	for(interface = interface_list; interface != NULL; interface = interface->ifa_next) {
		if(interface->ifa_addr->sa_family == AF_PACKET && strcmp(interface->ifa_name, "lo") != 0) {
			// mac address is in AF_PACKET interfaces
			// we also need to skip the loopback device because its mac is always just zeros
			struct sockaddr_ll* address = (struct sockaddr_ll*)interface->ifa_addr;
			memcpy(buffer, address->sll_addr, 6);
			id_set = 1;
			LOGD("id generation succeeded from interface: %s\n", interface->ifa_name);
			break;
		}
	}
	freeifaddrs(interface_list);

	if(id_set == 0) {
		LOGD("id generation failed, trying random number\n");
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
void send_ipv4_broadcast(char sender_id[6]) {
	int broadcast_socket = socket(AF_INET6, SOCK_DGRAM, 0);
	if(broadcast_socket == -1) {
		LOGD("socket %s\n", strerror(errno));
	}

	if(setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0) {
		LOGD("setsockopt: %s\n", strerror(errno));
	}

	struct ifaddrs* interface_list;
	int success;
	if((success = getifaddrs(&interface_list)) != 0) {
		LOGD("getifaddrs: %s\n", gai_strerror(success));
		return;
	}

	// now we can iterate over the interfaces
	struct ifaddrs* interface;
	for(interface = interface_list; interface != NULL; interface = interface->ifa_next) {
		if(interface->ifa_addr == NULL || interface->ifa_addr->sa_family != AF_INET) {
			// only consider interfaces that have a valid ipv4 address
			continue;
		}

		// now we have to calculate the broadcast address for the interface
        struct sockaddr_in ipv4_address;
        memset(&ipv4_address, 0, sizeof(ipv4_address));
        ipv4_address.sin_addr.s_addr = ((struct sockaddr_in*)interface->ifa_addr)->sin_addr.s_addr | (~((struct sockaddr_in*)interface->ifa_netmask)->sin_addr.s_addr);
        ipv4_address.sin_family = AF_INET;
        ipv4_address.sin_port = htons(BROADCAST_LISTENER_PORT);

        packet_type packet;
        memcpy(packet.protocol_id, "P2PFSYNC", 8);
        packet.message_type = MESSAGE_TYPE_DISCOVER;
        memcpy(packet.sender_id, sender_id, 6);
        if(sendto(broadcast_socket, &packet, sizeof(packet), 0, (struct sockaddr*)&ipv4_address, sizeof(ipv4_address)) == -1) {
            LOGD("sendto: %s\n", strerror(errno));
        }
	}
	freeifaddrs(interface_list);
}

// the multicast is sent once to the multicast address
// I am not sure atm if this needs to be done per interface
// for ipv4 it is easy as every interface has a different
// ipv4 broadcast address. The multicast address for ipv6
// is unique as far as i know
// there are actually ways to set the interface for sending
// ipv6 multicasts but it is not trivial. Idk if it is even possible
// without root priviliges so we will stick with a single multicast here
void send_ipv6_multicast(char sender_id[6]) {
	int multicast_socket = socket(AF_INET6, SOCK_DGRAM, 0);
	if(multicast_socket == -1) {
		LOGD("socket %s\n", strerror(errno));
	}

	struct sockaddr_in6 ipv6_address;
	memset(&ipv6_address, 0, sizeof(ipv6_address));
	ipv6_address.sin6_family = AF_INET6;
	ipv6_address.sin6_port = htons(BROADCAST_LISTENER_PORT);
	inet_pton(AF_INET6, IPV6_MULTICAST_ADDRESS, &ipv6_address.sin6_addr);

	packet_type packet;
    memcpy(packet.protocol_id, "P2PFSYNC", 8);
	packet.message_type = MESSAGE_TYPE_DISCOVER;
	memcpy(packet.sender_id, sender_id, 6);
	if(sendto(multicast_socket, &packet, sizeof(packet), 0, (struct sockaddr*)&ipv6_address, sizeof(ipv6_address)) == -1) {
		LOGD("sendto: %s\n", strerror(errno));
	}
}

int send_available_packet(struct sockaddr* destination_address, char sender_id[6]) {
	packet_type packet;
	memset(&packet, 0, sizeof(packet_type));
	memcpy(packet.protocol_id, "P2PFSYNC", 8);
	packet.message_type = MESSAGE_TYPE_AVAILABLE;
	memcpy(packet.sender_id, sender_id, 6);

	int sender_socket = socket(destination_address->sa_family, SOCK_DGRAM, 0);
	if(sender_socket == -1) {
		LOGD("socket: %s\n", strerror(errno));
		return -1;
	}
    if(destination_address->sa_family == AF_INET6) {
        ((struct sockaddr_in6*)destination_address)->sin6_port = htons(BROADCAST_LISTENER_PORT);
    } else if(destination_address->sa_family == AF_INET) {
        ((struct sockaddr_in*)destination_address)->sin_port = htons(BROADCAST_LISTENER_PORT);
    }

	socklen_t destinationAddressSize = destination_address->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
	if(sendto(sender_socket, &packet, sizeof(packet), 0, destination_address, destinationAddressSize) == -1) {
		LOGD("sendto: %s\n", strerror(errno));
		return -1;
	}
	//logger("sent an available packet\n");
	return 0;
}

int is_packet_valid(packet_type* packet) {
	if(memcmp(packet->protocol_id, "P2PFSYNC", 8) != 0) {
		return 0;
	}
	switch((unsigned int)(packet->message_type)) {
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
int create_broadcast_listener() {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* result_list;
	int success = getaddrinfo(NULL, BROADCAST_LISTENER_PORT_STRING, &hints, &result_list);
	if(success != 0) {
		LOGD("getaddrinfo: %s\n", gai_strerror(success));
		return -1;
	}

	int listener_socket = -1;
	struct addrinfo* iterator;

	for(iterator = result_list; iterator != NULL; iterator = iterator->ai_next) {
		// first try to get IPv6 address
		if(iterator->ai_family != AF_INET6) {
			continue;
		}
		listener_socket = socket(iterator->ai_family, iterator->ai_socktype, iterator->ai_protocol);
		if(listener_socket == -1) {
			LOGD("socket: %s\n", strerror(errno));
			continue;
		}
		if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
			LOGD("setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
			continue;
		}
		// try to join the ipv6 multicast group
		if(bind(listener_socket, iterator->ai_addr, iterator->ai_addrlen) == 0) {
            // join membership
            struct ipv6_mreq group;
            group.ipv6mr_interface = 0;
            inet_pton(AF_INET6, IPV6_MULTICAST_ADDRESS, &group.ipv6mr_multiaddr);
            if(setsockopt(listener_socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &group, sizeof(group)) == -1) {
            	LOGD("setsockopt %s\n", strerror(errno));
            }
			break;
		}
		close(listener_socket);
		listener_socket = -1;
	}
	if(listener_socket == -1) {
		LOGD("no ipv6 listener could be created, trying for ipv4 instead\n");
		// try to get ipv4 instead
		for(iterator = result_list; iterator != NULL; iterator = iterator->ai_next) {
			if(iterator->ai_family != AF_INET) {
				continue;
			}
			listener_socket = socket(iterator->ai_family, iterator->ai_socktype, iterator->ai_protocol);
			if(listener_socket == -1) {
				LOGD("socket: %s\n", strerror(errno));
				continue;
			}
			if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int)) < 0) {
				LOGD("setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
			}
			if(bind(listener_socket, iterator->ai_addr, iterator->ai_addrlen) == 0) {
				break;
			}
			close(listener_socket);
			listener_socket = -1;
		}
	}
	return listener_socket;
}
