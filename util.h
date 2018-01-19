/**
 * @file util.h
 * @brief This file provides utility functions.
 *
 * All functionality that is not exclusive to a module is in this file. The functions mostly consists of socket helper functions and string formatting functions.
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>

/**
 * @brief Connect a socket with a timeout
 *
 * @param address The destination address
 * @param port The destination port
 * @param timeout_seconds The timeout in seconds
 * @return If the connection succeeds the socket file descriptor is returned. If it fails -1 is returned.
 */
int connect_with_timeout(const struct sockaddr* address, unsigned short port, double timeout_seconds);

/**
 * @brief Creates a tcp listener socket that can listen on the specified port.
 *
 * After creation listen() should be called.
 *
 * @param port The port to listen on
 * @return If the socket is created successfully the socket file descriptor is returned. If it fails -1 is returned.
 */
int create_tcp_listener(const char* port);

/**
 * @brief Creates a udp listener socket that can listen on the specified port.
 *
 * After creation listen() should be called.
 *
 * @param port The port to listen on
 * @return If the socket is created successfully the socket file descriptor is returned. If it fails -1 is returned.
 */
int create_udp_listener(const char* port);

/**
 * @brief Prints the contents of @p in_buffer to @p buffer formatted as a hex string.
 *
 * As one byte maps to two characters @p buffer_size should be >= 2 * @p count + 1.
 * Otherwise the output will be truncated.
 *
 * @param in_buffer The memory area to print to the buffer
 * @param count How many bytes to print
 * @param buffer The buffer to print the hex string to
 * @param buffer_size The size of @p buffer
 */
void get_hex_string(const unsigned char* in_buffer, const size_t count, char* buffer, const size_t buffer_size);

/**
 * @brief Creates a readable string of an ip address
 *
 * The ip address contained in @p address is written to @p buffer as
 * [IPV4 <ipv4 address>] if it is an IPv4 address or as
 * [IPV6 <ipv6 address>] if it is an IPv6 address.
 *
 * @param address The address to print
 * @param buffer The buffer to print to
 * @param buffer_size The size of @p buffer
 */
void get_ip_address_string_prefixed(struct sockaddr* address, char* buffer, const size_t buffer_size);

/**
 * @brief Returns how much time passed since a given point in time
 *
 * @param t The point in time
 * @return The time that passed since @p t in seconds.
 */
double get_passed_time(struct timeval t);

/**
 * @brief Checks if a given IPv6 address is a mapped IPv4 address
 *
 * @param address The address to check
 * @return If the given address is a mapped IPv4 address 1 is returned. Otherwise 0 is returned.
 */
int is_ipv4_mapped(struct sockaddr* address);

/**
 * @brief Creates a directory structure including parent directories
 *
 * This function is the equivalent of "mkdir -p".
 *
 * Usage example:
 * @code
 * mkdirp("./sync_files/sub1/deeper/really_deep/");
 * @endcode
 * This will create the directories sync_files, sub1, deeper, really_deep so that
 * after the call ./sync_files/sub1/deeper/really_deep/ is accessible.
 *
 * @param path The path that describes the directory structure to create
 */
void mkdirp(const char* path);

/**
 * @brief Receives a length prefixed tcp "message"
 *
 * As tcp only sends streams but sometimes packets or messages are more desirable
 * this function provides a way receive a length prefixed tcp message. This function
 * can receive messages sent with tcp_message_send().
 *
 * @param socketfd The socket to use for receiving
 * @param buffer The buffer to write the received data to
 * @param buffer_size The size of the buffer
 * @param timeout_seconds The maximum wait time before returning with an error
 * @return If successful returns how many bytes were received. Otherwise -1 or 0 is returned.
 */
int tcp_message_receive(int socketfd, char* buffer, size_t buffer_size, double timeout_seconds);

/**
 * @brief Sends a length prefixed tcp "message"
 *
 * As tcp only sends streams but sometimes packets or messages are more desirable
 * this function provides a way receive a length prefixed tcp message. Messages sent with
 * this function can be received with tcp_message_receive().
 *
 * @param socketfd The socket to use for sending
 * @param buffer The data to send
 * @param buffer_size The count of bytes to send
 * @param timeout_seconds The maximum wait time before returning with an error
 * @return If successful returns 1. Otherwise -1 or 0 is returned.
 */
int tcp_message_send(int socketfd, char* buffer, uint32_t buffer_size, double timeout_seconds);

#endif
