/**
 * @file logger.h
 * @brief This module exposes logging functionality.
 *
 * The logger can be set to different levels so low priority messages can be filtered out beforehand.
 * Thread safe access and convenience macros are provided to enable easy logging. On startup the logger
 * will create a log file in the directory "logs" named with the current date.
 */

#ifndef LOGGER_H
#define LOGGER_H

/**
 * @brief Initialize the mutex for logging. This has to be the first call to this module to work correctly
 */
void initialize_logger_lock();

/**
 * @brief Destroys the mutex for logging. This should be called when the logger is not needed anymore.
 */
void destroy_logger_lock();

/// Log levels
typedef enum {
	LOG_DEBUG = 0,  //!< All messages are logged including Debug messages
	LOG_INFO = 1,   //!< No Debug messages are logged
	LOG_WARNING = 2,//!< No Debug or Info messages are logged
	LOG_ERROR = 3   //!< Only Error messages are logged
} log_level_t;

/**
 * @brief Set the log level for future calls to the logger.
 * @param level One of ::log_level_t.
 */
void set_log_level(log_level_t level);

// the logger function does not need to be called directly
// instead use one of the following macros
#define LOGD(FORMAT, ...) logger(LOG_DEBUG, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGI(FORMAT, ...) logger(LOG_INFO, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGW(FORMAT, ...) logger(LOG_WARNING, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)
#define LOGE(FORMAT, ...) logger(LOG_ERROR, __FILE__, __FUNCTION__, FORMAT, ##__VA_ARGS__)

/**
 * @brief This is the threadsafe logging function.
 *
 * This function should not be called directly. Instead one should use one of the LOG macros defined in this header.
 * The level, file and function are inserted automatically when one of the macros is used so the macros can be used
 * analogous to printf. Example:
 * \code{.c}
 * LOGD("received %d bytes %s\n", count, buffer);
 * \endcode
 * @param level The log level of this log call. Only if this is greater or equal to the currently set log level.
 * @param file The file containing the function that issued the log call. This is inserted automatically if one of the LOG macros is used.
 * @param function The function that issued the log call. This is inserted automatically if one of the LOG macros is used.
 * @param format_string The format string. This is passed on to printf. After this additional arguments can be passed, see the example.
 */
void logger(log_level_t level, const char* file, const char* function, const char* format_string, ...);

#endif
