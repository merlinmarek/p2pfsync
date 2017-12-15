#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <sys/types.h>

int createUDPListener(const char* port);
int createTCPListener(const char* port);
void printIpAddress(const struct sockaddr* address);
void printIpAddressFormatted(struct sockaddr* address);
int isIpv4Mapped(struct sockaddr* address);

#endif
