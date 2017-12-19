#include <string.h>
#include <unistd.h>

#include "file_transfer.h"
#include "logger.h"
#include "shutdown.h"

void* file_transfer_thread(void* tid) {
	LOGD("file_transfer_thread started\n");
	while(!get_shutdown()) {
		sleep(1);
	}
	LOGD("file_transfer_thread ended\n");
	return NULL;
}
