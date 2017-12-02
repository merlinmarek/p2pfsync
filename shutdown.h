#ifndef SHUTDOWN_H
#define SHUTDOWN_H

// these mutex functions MUST be called
void initializeShutdownLock();
void destroyShutdownLock();

void setShutdown(int shutdown);
int getShutdown();

#endif
