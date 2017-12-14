#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netdb.h>
#include <pthread.h>
#include <ifaddrs.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/time.h>

#include "defines.h"
#include "logger.h"
#include "shutdown.h"
#include "broadcast.h"
#include "socketUtil.h"
#include "peerList.h"


// the packets need to carry an id as well so we can match ipv4 and ipv6 endpoints as well
// as multiple connection path to some peer
// stackoverflow proposed the mac address of any device that it connected to the network
// so we will use the mac of the first device that has a valid ipv4/ipv6 address and is not the
// loopback device!

void sigintHandler(int unused) {
	logger("\nreceived SIGINT, shutting down...\n");
	setShutdown(1);
}

// gives the difference t1 - t2 in milliseconds
double getTimeDifferenceMs(struct timeval t1, struct timeval t2) {
    double elapsedTime = (t1.tv_sec - t2.tv_sec) * 1000.0; // sec to ms
    elapsedTime += (t1.tv_usec - t2.tv_usec) / 1000.0;		// us to ms
    return elapsedTime;
}

// gives the difference now - t
double getPassedTime(struct timeval t) {
	struct timeval now;
	gettimeofday(&now, 0);
	return getTimeDifferenceMs(now, t);
}

// the broadcasting thread is responsible for continously discovering
// new peers and responding to discovery packets sent by other peers
// this status should be in the peerList
// whenever the broadcastThread discovers a new peer it should add it to the
// peerList
// discovery broadcast packets are sent in a fixed interval for now like 20 sec
void* broadcastThread(void* tid) {
	logger("broadcastThread started\n");

	int broadcastListener = createBroadcastListener();

	// we also need our id so other clients can match ip addresses
	char ownId[6];
	getOwnId(ownId);

	logger("own id is: %02x%02x%02x%02x%02x%02x\n",
			(unsigned char)ownId[0],
			(unsigned char)ownId[1],
			(unsigned char)ownId[2],
			(unsigned char)ownId[3],
			(unsigned char)ownId[4],
			(unsigned char)ownId[5]);

	// initialize timer here so the first broadcast is done after the first timeout
	struct timeval lastBroadcastDiscovery;
	gettimeofday(&lastBroadcastDiscovery, NULL);

	fd_set master_fds;
	FD_ZERO(&master_fds);
	FD_SET(broadcastListener, &master_fds);

	logger("listening to broadcast @ %d\n", broadcastListener);

	while(!getShutdown()) {
		if(getPassedTime(lastBroadcastDiscovery) > 10000.0) {
			// we want to rebroadcast after 5 seconds are over
			// this should probably be random so the chance for collisions is low?!
			// should this be something I need to think about with a layer 4 protocol like udp
			// or is this automagically done by the lower layers?
			gettimeofday(&lastBroadcastDiscovery, NULL); // reset the timer
            sendIpv6Multicast(ownId);
            sendIpv4Broadcasts(ownId);
		}

	    fd_set read_fds = master_fds;
	    struct timeval timeout;
	    memset(&timeout, 0, sizeof(timeout));
	    timeout.tv_sec = 1; // block at maximum one second at a time
	    int success = select(broadcastListener + 1, &read_fds, NULL, NULL, &timeout);
	    if(success == -1) {
	    	logger("select: %s\n", strerror(errno));
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
	    	logger("recvfrom failed %s\n", strerror(errno));
	    	continue;
	    }
	    if(receivedBytes == 0) {
	    	logger("got zero bytes :/\n");
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
            	logger("["); printSenderId(packet->senderId); logger("]");
            	logger("[DISCOVERY]");
            	logger("["); printIpAddressFormatted((struct sockaddr*)&otherAddress); logger("]");
            	logger("\n");
            	sendAvailablePacket((struct sockaddr*)&otherAddress, ownId);
            	break;
            case MESSAGE_TYPE_AVAILABLE:
            	// when someone responded to our discovery request we should store their online status somewhere !!
            	logger("["); printSenderId(packet->senderId); logger("]");
            	logger("[AVAILABLE]");
            	logger("["); printIpAddressFormatted((struct sockaddr*)&otherAddress); logger("]");
            	logger("\n");
            	break;
            default:
            	// this should never happen
            	break;
            }
	    } else {
	    	// either we did receive a wrong amount of bytes or printPacket failed (success == 0)
	    	logger("received %d bytes: %*.*s\n", receivedBytes, 0, receivedBytes, receiveBuffer);
	    }
	}

	//cleanup
	close(broadcastListener);
	logger("broadcastThread ended\n");
	return NULL;
}

void* commandThread(void* tid) {
	logger("commandThread started\n");
	while(!getShutdown()) {
		sleep(1);
	}
	logger("commandThread ended\n");
	return NULL;
}

void* fileTransferThread(void* tid) {
	logger("fileTransferThread started\n");
	while(!getShutdown()) {
		sleep(1);
	}
	logger("fileTransferThread ended\n");
	return NULL;
}


int main() {
	/*
	 * 1. open a broadcast listener so we can respond to other peers
	 * 2. get broadcast ips from all interfaces and send broadcast packet
	 * 3. hopefully we discover some clients this way
	 * 4. lalala
	 * 5. ...
	 */
	// first initialize all the locks
	initializeShutdownLock();
	initializeLoggerLock();
	initializePeerListLock();

	setShutdown(0); // make sure we do not shutdown right after starting

	pthread_t broadcastThreadId;
	pthread_t commandThreadId;
	pthread_t fileTransferThreadId;

	int success;

	success = pthread_create(&broadcastThreadId, NULL, broadcastThread, (void*)0);
	if(success != 0) {
		logger("pthread_create failed with return code %d\n", success);
	}
	success = pthread_create(&commandThreadId, NULL, commandThread, (void*)0);
	if(success != 0) {
		logger("pthread_create failed with return code %d\n", success);
	}
	success = pthread_create(&fileTransferThreadId, NULL, fileTransferThread, (void*)0);
	if(success != 0) {
		logger("pthread_create failed with return code %d\n", success);
	}

	// setup sigint handler
	signal(SIGINT, sigintHandler);

	while(!getShutdown()) {
		sleep(1);
	}

	// join all threads
	pthread_join(broadcastThreadId, NULL);
	pthread_join(commandThreadId, NULL);
	pthread_join(fileTransferThreadId, NULL);

	logger("threads are down\n");
	// cleanup
	freePeerList();

	// destroy all locks
	destroyShutdownLock();
	destroyPeerListLock();
	destroyLoggerLock();

	pthread_exit(NULL); // should be at end of main function
}
