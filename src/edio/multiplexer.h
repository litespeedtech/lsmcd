/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef MULTIPLEX_H
#define MULTIPLEX_H

#include <lsdef.h>
/**
  *@author George Wang
  */
#include <edio/eventreactor.h>

class Multiplexer
{
    int m_iFLTag;
protected:
    Multiplexer();
public:
    virtual ~Multiplexer() {};
    enum
    {
        DEFAULT_CAPACITY = 16
    };
    virtual int getHandle() const          {    return -1;  }
    virtual int init(int capacity = DEFAULT_CAPACITY) { return 0; };
    virtual int add(EventReactor *pHandler, short mask) = 0;
    virtual int remove(EventReactor *pHandler) = 0;
    virtual int waitAndProcessEvents(int iTimeoutMilliSec) = 0;
    virtual void timerExecute() = 0;
    virtual void setPriHandler(EventReactor::pri_handler handler) = 0;
    virtual int processEvents() = 0;

    virtual void continueRead(EventReactor *pHandler);
    virtual void suspendRead(EventReactor *pHandler);
    virtual void continueWrite(EventReactor *pHandler);
    virtual void suspendWrite(EventReactor *pHandler);
    virtual void switchWriteToRead(EventReactor *pHandler);
    virtual void switchReadToWrite(EventReactor *pHandler);

    int  getFLTag() const   {   return m_iFLTag;        }
    void setFLTag(int tag)  {   m_iFLTag = tag;         }


};

#endif
