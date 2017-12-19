#ifndef COMMAND_H
#define COMMAND_H

// this module only exposes the commandThread
// for this to work correctly the peerList has to be setup beforehand
void* commandThread(void* tid);

#endif
