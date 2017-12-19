#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

int createUDPListener(const char* port);
int createTCPListener(const char* port);
void printIpAddress(const struct sockaddr* address);
void printIpAddressFormatted(struct sockaddr* address);
int isIpv4Mapped(struct sockaddr* address);
void printHex(const unsigned char* buffer, const int count);
double getTimeDifferenceMs(struct timeval t1, struct timeval t2);
double getPassedTime(struct timeval t);

#endif
