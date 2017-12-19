#ifndef BROADCAST_H
#define BROADCAST_H

// this module only exposes the broadcastThread
// for this to work correctly the peerList has to be setup beforehand
void* broadcastThread(void* tid);

#endif
