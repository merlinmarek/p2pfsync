#ifndef SOCKET_UTIL_H
#define SOCKET_UTIL_H

#include <sys/types.h>

int createUDPListener(const char* port);
int createTCPListener(const char* port);
void printIpAddress(const struct sockaddr* address);

#endif
