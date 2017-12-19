#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

int create_udp_listener(const char* port);
int create_tcp_listener(const char* port);
void get_ip_address_formatted(struct sockaddr* address, char* buffer, const size_t buffer_size);
int is_ipv4_mapped(struct sockaddr* address);
void get_hex_string(const unsigned char* in_buffer, const size_t count, char* buffer, const size_t buffer_size);
double get_time_difference_ms(struct timeval t1, struct timeval t2);
double get_passed_time(struct timeval t);

#endif
