#ifndef LOGGER_H
#define LOGGER_H

// these mutex functions MUST be called
void initializeLoggerLock();
void destroyLoggerLock();

typedef enum {
	LOG_DEBUG = 0,
	LOG_INFO = 1,
	LOG_WARNING = 2,
	LOG_ERROR = 3
} LogLevel;

// use this to set the log level
// default is LOG_DEBUG
void set_log_level(LogLevel level);

// the logger function does not need to be called directly
// instead use one of the following macros
#define LOGD(FORMAT, ...) logger(LOG_DEBUG, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGI(FORMAT, ...) logger(LOG_INFO, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGW(FORMAT, ...) logger(LOG_WARNING, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGE(FORMAT, ...) logger(LOG_ERROR, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)

// thread safe (hopefully) logging function
// example call
// logger(LOG_DEBUG, "broadcast_thread", "send_available_packet", "received %d bytes\n", receivedBytes);
void logger(LogLevel level, const char* file, const char* function, const char* formatString, ...);

#endif
