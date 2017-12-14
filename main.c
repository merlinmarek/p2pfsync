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
#include <sys/stat.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/time.h>

#include "defines.h"
#include "logger.h"
#include "shutdown.h"
#include "broadcast.h"
#include "socketUtil.h"
#include "peerList.h"
#include "md5.h"


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

            printPeerList();
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


            	// we have seen the peer just now
            	logger("");
            	struct timeval lastSeen;
            	gettimeofday(&lastSeen, NULL);
            	addIpToPeer(packet->senderId, (struct sockaddr*)&otherAddress, lastSeen);

            	/*
            	 * THIS DOES NOT BELONG HERE! WE NEED TO SOMEHOW SIGNAL TO THE COMMAND THREAD
            	 * THAT THERE IS A NEW CLIENT
            	// send our id :)
            	logger("trying to send\n");
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
            		logger("connect %s\n", strerror(errno));
            	}
            	if(send(commandfd, asdf, 6, 0) < 0) {
            		logger("send %s\n", strerror(errno));
            	}

            	close(commandfd);
            	logger("closed\n");
            	*/


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

	// the command thread needs to have an in-memory representation of the file system
	// the default sync folder is ./sync_folder so all paths are relative to this folder
	// this makes a request for /test go to ./sync_folder/test
	// the command thread should then report all files/subdirectories in the requested directory


	// first we need to init the internal file system structure
	// whether or not this needs custom classes I don't know atm
	// I will first try without
	// later with inotify this thread should also watch for any file changes
	// the internal representation works like this:
	// every leaf node is assigned a hash of its contents. in the case of an empty directory
	// this should be some fixed value like 0. Then the hash for a directory is the hash of
	// the hashes of all its contents. The hashes are concatenated sorted by file names (like ls)
	// this gives something similar to a merkle tree. With this it is easy to check whether there are
	// differences in the directories
	// for example:
	// a newly connected client can just ask for the hash of the root directory. If this is the same
	// as he already has, he does not have to make any more requests because he knows that he is in sync

	// this is all to much for tonight so we go for enumeration of a requested directory only FOR NOW
	// a peer that has discovered us can establish a command connection to us and then send a
	// GET / request to us
	// GET <directory>
	// we look up ./sync_folder/<directory> and return the list of files/directories in this folder
	// for hashing we will use md5

	int listenerSocket = createTCPListener(COMMAND_LISTENER_PORT_STRING);
	if(listen(listenerSocket, 10) != 0) {
		logger("listen %s\n", strerror(errno));
	}

	fd_set master_fds;
	FD_ZERO(&master_fds);
	FD_SET(listenerSocket, &master_fds);

	int maxfd = listenerSocket;

	while(!getShutdown()) {
	    fd_set read_fds = master_fds;
	    struct timeval timeout;
	    memset(&timeout, 0, sizeof(timeout));
	    timeout.tv_sec = 1; // block at maximum one second at a time
	    int success = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
	    if(success == -1) {
	    	logger("select: %s\n", strerror(errno));
	    	continue;
	    }
	    if(success == 0) {
	    	// timed out
	    	continue;
	    }
	    // if we reach this point we can accept an incoming connection
	    char receiveBuffer[128];
	    struct sockaddr_storage otherAddress;
	    socklen_t otherLength = sizeof(otherAddress);

	    int remotefd = accept(listenerSocket, (struct sockaddr*)&otherAddress, &otherLength);
	    int receivedBytes = recv(remotefd, receiveBuffer, sizeof(receiveBuffer), 0);
	    logger("got message from: %.6s\n", receiveBuffer);
	    close(remotefd);
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

void fileMD5(unsigned char result[16], const char* filePath) {
	// testing md5 here
	MD5_CTX md5hash;
	MD5_Init(&md5hash);

	// this assumes that the file is small enough to fit entirely in ram
	struct stat st;
	if(stat(filePath, &st) != 0) {
		// the file does not exist
		logger("file %s does not exist\n", filePath);
		return;
	}
	char* buffer = (char*)malloc(st.st_size);
	int filefd = open(filePath, O_RDONLY);
	if(filefd == -1) {
		logger("error opening file\n");
	}
	read(filefd, buffer, st.st_size);
	MD5_Update(&md5hash, (void*)buffer, st.st_size);
	MD5_Final(result, &md5hash);
	free(buffer);
}


int main() {
	/*unsigned char result[16];
	memset(result, 0, 16);

	fileMD5(result, "./main.c");

	printf("hash is: ");
	int i;
	for(i = 0; i < 16; i++) {
		printf("%02x", result[i]);
	}
	printf("\n");
	return 0;
	*/

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
