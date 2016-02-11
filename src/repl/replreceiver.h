#ifndef REPLRECEIVER_H
#define REPLRECEIVER_H

#include <stdint.h>
#include <lsdef.h>
#include <util/tsingleton.h>

class ReplContainer;
class ReplConn;
struct ReplPacketHeader;

class ReplReceiver
{
public:
    ReplReceiver();
    virtual ~ReplReceiver();

    virtual int processPacket ( ReplConn * pConn, ReplPacketHeader * header, char * pData )=0;
    

    LS_NO_COPY_ASSIGN(ReplReceiver);
};

ReplReceiver * getReplReceiver();
void setReplReceiver(ReplReceiver *pReceiver);

#endif // REPLRECEIVER_H
