#ifndef LOGGER_H
#define LOGGER_H

// these mutex functions MUST be called
void initializeLoggerLock();
void destroyLoggerLock();

// thread safe (hopefully) logging function
void logger(const char* formatString, ...);

#endif
