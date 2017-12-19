#ifndef SHUTDOWN_H
#define SHUTDOWN_H

// these mutex functions MUST be called
void initialize_shutdown_lock();
void destroy_shutdown_lock();

void set_shutdown(int shutdown);
int get_shutdown();

#endif
