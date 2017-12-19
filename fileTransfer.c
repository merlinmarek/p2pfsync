#include <string.h>
#include <unistd.h>

#include "fileTransfer.h"
#include "logger.h"
#include "shutdown.h"

void* fileTransferThread(void* tid) {
	LOGD("fileTransferThread started\n");
	while(!getShutdown()) {
		sleep(1);
	}
	LOGD("fileTransferThread ended\n");
	return NULL;
}
