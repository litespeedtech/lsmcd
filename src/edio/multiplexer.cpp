/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <edio/multiplexer.h>
#include <fcntl.h>
#include <sys/poll.h>

Multiplexer::Multiplexer()
    : m_iFLTag(O_NONBLOCK | O_RDWR)
{}

void Multiplexer::continueRead(EventReactor *pHandler)
{   pHandler->orMask2(POLLIN);    }

void Multiplexer::suspendRead(EventReactor *pHandler)
{   pHandler->andMask2(~POLLIN);  }

void Multiplexer::continueWrite(EventReactor *pHandler)
{   pHandler->orMask2(POLLOUT);   }

void Multiplexer::suspendWrite(EventReactor *pHandler)
{   pHandler->andMask2(~POLLOUT); }

void Multiplexer::switchWriteToRead(EventReactor *pHandler)
{   pHandler->setMask2(POLLIN | POLLHUP | POLLERR);   }

void Multiplexer::switchReadToWrite(EventReactor *pHandler)
{   pHandler->setMask2(POLLOUT | POLLHUP | POLLERR);   }


