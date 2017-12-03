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


void sigintHandler(int unused) {
	logger("\nreceived SIGINT, shutting down...\n");
	setShutdown(1);
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
	logger("listening to broadcast @ %d\n", broadcastListener);
	//sendBroadcastDiscover(); // initial discovery
	sendIpv6Multicast();
	sendIpv4Broadcasts();
	struct timeval lastBroadcastDiscovery;
	gettimeofday(&lastBroadcastDiscovery, NULL);

	fd_set master_fds;
	FD_ZERO(&master_fds);
	FD_SET(broadcastListener, &master_fds);

	while(!getShutdown()) {
		// check if we need to do another broadcast
		/*struct timeval now;
		gettimeofday(&now, NULL);
		double elapsedTime = (now.tv_sec - lastBroadcastDiscovery.tv_sec) * 1000.0; // sec to ms
		elapsedTime += (now.tv_usec - lastBroadcastDiscovery.tv_usec) / 1000.0;		// us to ms

		//logger("it has been %d seconds since last broadcast\n", (int)(elapsedTime / 1000.0));
		if(elapsedTime >= 10000.0) {
			logger("rebroadcasting after %d ms\n", (int)elapsedTime);
			gettimeofday(&lastBroadcastDiscovery, NULL);
			sendBroadcastDiscover();
		}
		*/

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
            if(isOwnAddress((struct sockaddr*)&otherAddress)) {
                // we do not want to deal with our own messages
                continue;
            }

            // when we reach this point this is a valid message from some other peer
            // check whether this is a discover request or an available message
            switch(packet->messageType) {
            case MESSAGE_TYPE_DISCOVER:
            	// we want to respond to a discovery request
            	logger("got discover packet from "); printIpAddress((struct sockaddr*)&otherAddress); logger("\n");
            	sendAvailablePacket((struct sockaddr*)&otherAddress);
            	break;
            case MESSAGE_TYPE_AVAILABLE:
            	// when someone responded to our discovery request we should store their online status somewhere !!
            	logger("received available packet from: "); printIpAddress((struct sockaddr*)&otherAddress); logger("\n");
            	break;
            default:
            	// this should never happen
            	break;
            }
	    } else {
	    	// either we did receive a wrong amount of bytes or printPacket failed (success == 0)
	    	logger("received %d bytes: %s\n", receivedBytes, receiveBuffer);
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

	logger("finally fuckers are down\n");
	// cleanup
	freePeerList();

	// destroy all locks
	destroyShutdownLock();
	destroyPeerListLock();
	destroyLoggerLock();

	pthread_exit(NULL); // should be at end of main function
}
