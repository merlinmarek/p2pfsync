#include <string.h>
#include <unistd.h>

#include "fileTransfer.h"
#include "logger.h"
#include "shutdown.h"

void* fileTransferThread(void* tid) {
	logger("fileTransferThread started\n");
	while(!getShutdown()) {
		sleep(1);
	}
	logger("fileTransferThread ended\n");
	return NULL;
}
