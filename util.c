#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "logger.h"

#include "util.h"

static int create_listener_socket(const char* port, int ai_socktype);
static double get_time_difference_seconds(struct timeval t1, struct timeval t2);
static int receive_tcp_n(int socketfd, char* buffer, size_t buffer_size, size_t n, double timeout_seconds);
static struct timeval timeval_from_double(double time_seconds);

// module structure should be
//
// top level includes sorted alphabetically
// 1 sublevel includes sorted alphabetically
// ...
// \n
// project includes sorted alphabetically
// \n
// module header include
// \n
// declaration of module scoped functions (static) sorted alphabetically
// \n
// implementation of modules declared in module header
// \n
// implementation of module scoped functions

int connect_with_timeout(const struct sockaddr* address, unsigned short port, double timeout_seconds) {
    // first we need to set the port
    if(address->sa_family == AF_INET) {
        // ipv4
        struct sockaddr_in* ipv4_address = (struct sockaddr_in*)address;
        ipv4_address->sin_port = htons(port);
    } else {
        // ipv6
        struct sockaddr_in6* ipv6_address = (struct sockaddr_in6*)address;
        ipv6_address->sin6_port = htons(port);
    }

    // now we create a non-blocking socket
    int socketfd = socket(address->sa_family, SOCK_STREAM, 0);

    if(fcntl(socketfd, F_SETFL, fcntl(socketfd, F_GETFL, 0) | O_NONBLOCK) == -1) {
        LOGE("fcntl set O_NONBLOCK\n");
        return -1;
    }

    int connect_return = connect(socketfd, address, address->sa_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));
    if(errno != EINPROGRESS && connect_return != 0) {
        LOGE("connect %s\n", strerror(errno));
        return -1;
    }

    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(socketfd, &write_set);
    struct timeval timeout;
    timeout.tv_sec = (int)timeout_seconds;
    timeout.tv_usec = (int)((timeout_seconds - (int)timeout_seconds) * 1000000);
    int select_return = select(socketfd + 1, NULL, &write_set, NULL, &timeout);
    if(select_return == -1) {
        // error
        LOGE("select %s\n", strerror(errno));
        return -1;
    }
    else if(select_return == 0) {
    	// timeout
        LOGD("connection attempt timed out\n");
        return -1;
    }
    else {
        // not timed out
        socklen_t option_length = sizeof(int);
        int option_value = 0;
        if(getsockopt(socketfd, SOL_SOCKET, SO_ERROR, (void*)(&option_value), &option_length) < 0) {
            LOGE("getsockopt() %s\n", strerror(errno));
            return -1;
        }
        if(option_value) {
            LOGE("error in delayed connection %s\n", strerror(option_value));
            return -1;
        }
        // connection established successfully
        // remove the O_NONBLOCK, otherwise all function calls fail
        if(fcntl(socketfd, F_SETFL, ~fcntl(socketfd, F_GETFL, 0) & O_NONBLOCK) == -1) {
            LOGE("fcntl clear O_NONBLOCK\n");
            return -1;
        }
    }
    return socketfd;
}

int create_tcp_listener(const char* port) {
	return create_listener_socket(port, SOCK_STREAM);
}

int create_udp_listener(const char* port) {
	return create_listener_socket(port, SOCK_DGRAM);
}

void get_hex_string(const unsigned char* in_buffer, const size_t count, char* buffer, const size_t buffer_size) {
	size_t i;
	for(i = 0; i < count && 2 * (i + 1) < buffer_size; i++) {
		sprintf(&buffer[2*i], "%02x", in_buffer[i]);
	}
	buffer[2 * i] = 0;
}

void get_ip_address_string_prefixed(struct sockaddr* address, char* buffer, size_t buffer_size) {
	size_t index = 0;
	if(address->sa_family == AF_INET) {
		// ipv4 address
		strncpy(&buffer[index], "IPv4 ", 5);
		index += 5;
		struct sockaddr_in* ipv4_address = (struct sockaddr_in*)address;
		inet_ntop(address->sa_family, &(ipv4_address->sin_addr), &buffer[index], buffer_size - index);
	} else if(address->sa_family == AF_INET6) {
		struct sockaddr_in6* ipv6_address = (struct sockaddr_in6*)address;
		if(is_ipv4_mapped(address)) {
			// this is an ipv6 mapped ipv4 address
			// the ipv4 address is stored in the last 4 bytes of the 16 byte ipv6 address
            strncpy(&buffer[index], "IPv4 ", 5);
            index += 5;
			char* ipv4AddressStart = ((char*)(&ipv6_address->sin6_addr)) + 12;
			struct in_addr* ipv4_address = (struct in_addr*)ipv4AddressStart;
			inet_ntop(AF_INET, ipv4_address, &buffer[index], buffer_size - index);
		} else {
			// regular ipv6 address
            strncpy(&buffer[index], "IPv6 ", 5);
            index += 5;
			inet_ntop(AF_INET6, &(ipv6_address->sin6_addr), &buffer[index], buffer_size - index);
		}
	} else {
		strncpy(&buffer[index], "unkown address family", buffer_size - index);
	}
	// make sure that the buffer is 0 terminated
	buffer[buffer_size - 1] = 0;
}

// gives the difference now - t in seconds!
double get_passed_time(struct timeval t) {
	struct timeval now;
	gettimeofday(&now, 0);
	return get_time_difference_seconds(now, t);
}

// returns whether some ipv6 address is actually an ipv6 mapped
// ipv4 address
int is_ipv4_mapped(struct sockaddr* address) {
	if(address->sa_family != AF_INET6) {
		return 0;
	}
	struct sockaddr_in6* ipv6_address = (struct sockaddr_in6*)address;
	unsigned short* ipv6_address_words = (unsigned short*)(&ipv6_address->sin6_addr);
	if(ipv6_address_words[0] == 0 &&
		ipv6_address_words[1] == 0 &&
		ipv6_address_words[2] == 0 &&
		ipv6_address_words[3] == 0 &&
		ipv6_address_words[4] == 0 &&
		ipv6_address_words[5] == 0xffff) {
		return 1;
	}
	return 0;
}

// needs error checking!!
// needs error checking!!
void mkdirp(const char* path) {
	char* path_copy = strdup(path);
	char* p = NULL;
	for(p = path_copy + 1; *p; p++) {
		if(*p == '/') {
			*p = 0;
			mkdir(path_copy, S_IRWXU);
			*p = '/';
		}
	}
    mkdir(path_copy, S_IRWXU);
}

// this is a helper function to receive a length prefixed tcp message with a maximum length and a timeout, THIS IS A BLOCKING OPERATION
int tcp_message_receive(int socketfd, char* buffer, size_t buffer_size, double timeout_seconds) {
	// receive timeout will be handled with select
	// and a timer
	struct timeval start_time;
	gettimeofday(&start_time, NULL);

	char message_size_buffer[4];
	int receive_return = receive_tcp_n(socketfd, message_size_buffer, sizeof(message_size_buffer), 4, timeout_seconds);
	if(receive_return <= 0) {
		return receive_return;
	}
	if(receive_return != 4) {
		// we need EXACTLY 4 bytes...
		// if we do not get them we close the connection
		return 0;
	}
	uint32_t message_size = ntohl(*((uint32_t*)message_size_buffer));

	receive_return = receive_tcp_n(socketfd, buffer, buffer_size, message_size, 2.0);
	if(receive_return <= 0) {
		return receive_return;
	}
	if(receive_return != message_size) {
		// we are only interested in full messages
		// if we do not get a full message we close the connection
		return 0;
	}
	return receive_return;
}

// this is a helper function to send a length prefixed tcp message, THIS IS A BLOCKING OPERATION
int tcp_message_send(int socketfd, char* buffer, uint32_t buffer_size, double timeout_seconds) {
	timeout_seconds = 10000.0;
	// first set the send connection timeout parameter
	struct timeval send_timeout = timeval_from_double(timeout_seconds);
	/*if(setsockopt(socketfd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(struct timeval)) == -1) {
		// error printing is done by the calling thread
		return -1;
	}*/
	// first we need to send the size
	uint32_t network_buffer_size = htonl(buffer_size);
	int send_return = send(socketfd, (void*)&network_buffer_size, 4, 0);
	if(send_return <= 0) {
		// there was an error sending or the remote closed the connection
		// error printing should be done by the calling thread so we just return
		return send_return;
	}
	// now send the buffer
	size_t bytes_sent = 0;
	while(bytes_sent < buffer_size) {
        send_return = send(socketfd, buffer + bytes_sent, buffer_size - bytes_sent, 0);
        if(send_return <= 0) {
            // there was an error sending or the remote closed the connection
            // error printing should be done by the calling thread so we just return
            return send_return;
        }
        bytes_sent += send_return;
	}
	return 1; // return 0 = remote closed socket, return -1 = error
}

// MODULE SCOPED FUNTCIONS BEGIN

int create_listener_socket(const char* port, int ai_socktype) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = ai_socktype;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* result_list;
	int success = getaddrinfo(NULL, port, &hints, &result_list);
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
		if(bind(listener_socket, iterator->ai_addr, iterator->ai_addrlen) == 0) {
			break;
		}
		close(listener_socket);
		listener_socket = -1;
	}
	if(listener_socket == -1) {
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

// gives the difference t1 - t2 in seconds
double get_time_difference_seconds(struct timeval t1, struct timeval t2) {
    double elapsed_time = (t1.tv_sec - t2.tv_sec);
    elapsed_time += (t1.tv_usec - t2.tv_usec) / 1000000.0;
    return elapsed_time;
}

// another helper function to receive exactly n bytes of a tcp stream with a timeout used by receive_tcp_message
/*
 *
 *
 * THE TIMEOUT NEEDS FIXING
 * THE TIMEOUT NEEDS FIXING
 * THE TIMEOUT NEEDS FIXING
 * THE TIMEOUT NEEDS FIXING
 * THE TIMEOUT NEEDS FIXING
 * THE TIMEOUT NEEDS FIXING
 * THE TIMEOUT NEEDS FIXING
 * THE TIMEOUT NEEDS FIXING
 *
 *
 */
int receive_tcp_n(int socketfd, char* buffer, size_t buffer_size, size_t n, double timeout_seconds) {
	// receive timeout will be handled with select
	// and a timer
	struct timeval start_time;
	gettimeofday(&start_time, NULL);

	fd_set master_read_set;
	FD_ZERO(&master_read_set);
	FD_SET(socketfd, &master_read_set);

	int buffer_index = 0;

	while(buffer_index < buffer_size && buffer_index < n) {
		fd_set read_set = master_read_set;
		struct timeval select_timeout;
		select_timeout.tv_sec = (int)timeout_seconds;
		select_timeout.tv_usec = 0;
		int select_return = select(socketfd + 1, &read_set, NULL, NULL, &select_timeout);
		if(select_return == -1) {
			// on select
			LOGE("select %s\n", strerror(errno));
			return select_return;
		}
		double passed_time = get_passed_time(start_time);
        if(passed_time > timeout_seconds) {
            // timeout
        	//LOGD("select timedout\n");
            //return select_return;
        }
		// maybe the socket is ready :)
		if(FD_ISSET(socketfd, &read_set)) {
			// calculate the maximum bytes that still can be received
			int max_recv = buffer_size > n ? buffer_size - buffer_index : n - buffer_index;
			int recv_return = recv(socketfd, (void*)(buffer + buffer_index), max_recv - buffer_index, 0);
			if(recv_return <= 0) {
				// error or disconnected
				return recv_return;
			}
			buffer_index += recv_return;
		}
	}
	return buffer_index;
}

struct timeval timeval_from_double(double time_seconds) {
    struct timeval timeout;
    timeout.tv_sec = (int)time_seconds;
    timeout.tv_usec = (int)((time_seconds - (int)time_seconds) * 1000000);
    return timeout;
}
