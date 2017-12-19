#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "command.h"
#include "defines.h"
#include "logger.h"
#include "shutdown.h"
#include "util.h"

void* commandThread(void* tid) {
	LOGD("commandThread started\n");

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

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// this is all to much for tonight so we go for enumeration of a requested directory only FOR NOW
	// a peer that has discovered us can establish a command connection to us and then send a
	// GET / request to us
	// GET <directory>
	// we look up ./sync_folder/<directory> and return the list of files/directories in this folder
	// for hashing we will use md5
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	int listenerSocket = create_tcp_listener(COMMAND_LISTENER_PORT_STRING);
	if(listen(listenerSocket, 10) != 0) {
		LOGD("listen %s\n", strerror(errno));
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
	    	LOGD("select: %s\n", strerror(errno));
	    	continue;
	    }
	    if(success == 0) {
	    	// timed out
	    	continue;
	    }
	    // check which socket is ready
	    int i;
	    for(i = 0; i <= maxfd; i++) {
	    	if(FD_ISSET(i, &read_fds)) {
	    		// the socket is ready
	    		if(i == listenerSocket) {
	    			// we are ready to accept a new client
                    struct sockaddr_storage otherAddress;
                    socklen_t otherLength = sizeof(otherAddress);

                    int remotefd = accept(listenerSocket, (struct sockaddr*)&otherAddress, &otherLength);
                    // we should probably chech if this client is already in the peer list
                    // if he is not we should insert him there because we he must be a peer
                    if(remotefd == -1) {
                    	// accept failed
                    	LOGD("accept %s\n", strerror(errno));
                    	continue;
                    }
                    FD_SET(remotefd, &master_fds);
                    if(remotefd > maxfd) {
                    	// keep track of the maximum socket number
                    	maxfd = remotefd;
                    }
	    		} else {
	    			// this is a regular client socket that either closed the connection or wants something from us
                    char receiveBuffer[128];
                    int receivedBytes = recv(i, receiveBuffer, sizeof(receiveBuffer), 0);
                    if(receivedBytes == -1) {
                    	// error on recv
                    	// if this happens we want to close this socket and remove it from the master_fds
                    	LOGD("recv %s\n", strerror(errno));
                    	FD_CLR(i, &master_fds);
                    	continue;
                    }
                    if(receivedBytes == 0) {
                    	// connection closed by remote
                    	FD_CLR(i, &master_fds);
                    	continue;
                    }
                    if(receivedBytes < 5) {
                    	// a request needs a GET / as minimum
                    	continue;
                    }
                    receiveBuffer[receivedBytes] = ' '; // for strtok
                    const char* delim = " ";
                    const char* requestId = strtok(receiveBuffer, delim);
                    const char* requestPath = strtok(NULL, delim);
                    char* localPath = malloc(strlen(BASE_PATH) + strlen(requestPath) + 1);
                    memcpy(localPath, BASE_PATH, strlen(BASE_PATH));
                    strcpy(localPath + strlen(BASE_PATH), requestPath);
                    LOGD("id: %s, path: %s\n", requestId, localPath);

                    // now we enumerate files in the requested directory and return them as csv
                    char reply[8096];
                    memset(reply, 0, 8096);

                    struct stat info;

                    if(lstat(localPath, &info) != 0 || !S_ISDIR(info.st_mode)) {
                    	// the directory does not exist or is not a directory
                    	strcpy(reply, "requested invalid directory");
                    } else {
                    	// valid directory so list its contents
                    	DIR* directory;
                    	directory = opendir(localPath);
                    	if(!directory) {
                    		// error :/
                    	}
                    	int current_pos = 0;
                    	while(1) {
                            struct dirent* entry;
                            entry = readdir(directory);
                            if(!entry) {
                            	// there are no more entrys so we break
                            	break;
                            }
                            if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                            	// do not sent current and parent directory
                            	continue;
                            }
                            memcpy(reply + current_pos, entry->d_name, strlen(entry->d_name));
                            current_pos += strlen(entry->d_name);
                            reply[current_pos++] = '<'; // < is delimiter
                    	}
                    	reply[current_pos] = 0; // end of string instead of <
                    }
                    send(i, reply, strlen(reply), 0);

                    free(localPath);
	    		}
	    	}
	    }
	    // if we reach this point we can accept an incoming connection
		sleep(1);
	}
	// when we are done, we want to close all still open connections
	int sock;
	for(sock = 0; sock < maxfd; sock++) {
		if(FD_ISSET(sock, &master_fds)) {
			close(sock);

		}
	}
	LOGD("commandThread ended\n");
	return NULL;
}
