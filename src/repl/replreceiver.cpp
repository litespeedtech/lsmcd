
#include "replreceiver.h"
#include "nodeinfo.h"
#include "replpacket.h"
#include "replregistry.h"
#include "replsender.h"
#include "replconn.h"
#include "replicablelruhash.h"
#include "replgroup.h"
#include "replconf.h"
#include <log4cxx/logger.h>
#include <stddef.h>
#include <stdio.h>

ReplReceiver * g_pReplReceiver = NULL;
ReplReceiver * getReplReceiver()
{
    return g_pReplReceiver;
}

void setReplReceiver(ReplReceiver *pReceiver)
{
    assert(g_pReplReceiver == NULL);
    g_pReplReceiver = pReceiver;
}

ReplReceiver::ReplReceiver()
{}

ReplReceiver::~ReplReceiver()
{}
