#ifndef SHUTDOWN_H
#define SHUTDOWN_H

// this module wraps a single variable to provide a thread safe indicator whether the modules should shut down or not

// these mutex functions MUST be called
/**
 * @brief This function initializes the mutex for this module. This should be the first function that is called on this module
 */
void initialize_shutdown_lock();

/**
 * @brief When the shutdown module is not needed anymore its mutex should be destroyed by calling this function.
 */
void destroy_shutdown_lock();

/**
 * @brief Set the shutdown value (0/1). This is only called from the main thread.
 * @param shutdown The value to set to shutdown variable to. Should be 0 or 1. 0 => Should shutdown, 1 => Keep running
 */
void set_shutdown(int shutdown);

/**
 * @brief Returns the current value of the shutdown variable
 * @return The current value of the shutdown variable
 */
int get_shutdown();

#endif
