#ifndef LOGGER_H
#define LOGGER_H

// these mutex functions MUST be called
void initialize_logger_lock();
void destroy_logger_lock();

typedef enum {
	LOG_DEBUG = 0,
	LOG_INFO = 1,
	LOG_WARNING = 2,
	LOG_ERROR = 3
} log_level_t;

// use this to set the log level
// default is LOG_DEBUG
void set_log_level(log_level_t level);

// the logger function does not need to be called directly
// instead use one of the following macros
#define LOGD(FORMAT, ...) logger(LOG_DEBUG, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGI(FORMAT, ...) logger(LOG_INFO, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGW(FORMAT, ...) logger(LOG_WARNING, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGE(FORMAT, ...) logger(LOG_ERROR, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)

// thread safe (hopefully) logging function
void logger(log_level_t level, const char* file, const char* function, const char* format_string, ...);

#endif
