#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>

int connect_with_timeout(const struct sockaddr* address, unsigned short port, double timeout_seconds);
int create_tcp_listener(const char* port);
int create_udp_listener(const char* port);
void get_hex_string(const unsigned char* in_buffer, const size_t count, char* buffer, const size_t buffer_size);
void get_ip_address_string_prefixed(struct sockaddr* address, char* buffer, const size_t buffer_size);
double get_passed_time(struct timeval t);
int is_ipv4_mapped(struct sockaddr* address);
// this is a helper function to receive a length prefixed tcp message with a maximum length and a timeout, THIS IS A BLOCKING OPERATION
int tcp_message_receive(int socketfd, char* buffer, size_t buffer_size, double timeout_seconds);
// this is a helper function to send a length prefixed tcp message, THIS IS A BLOCKING OPERATION
int tcp_message_send(int socketfd, char* buffer, uint32_t buffer_size, double timeout_seconds);

#endif
