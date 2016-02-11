/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef FLOWCONTROL_H
#define FLOWCONTROL_H


/**
  *@author George Wang
  */
class InputFlowControl
{
public:
    InputFlowControl() {};
    virtual ~InputFlowControl() {};
    virtual void suspendRead() = 0;
    virtual void continueRead() = 0;
};

class OutputFlowControl
{
public:
    OutputFlowControl() {};
    virtual ~OutputFlowControl() {};

    virtual void suspendWrite() = 0;
    virtual void continueWrite() = 0;
};

class IOFlowControl : virtual public OutputFlowControl
    , virtual public InputFlowControl
{
public:
    IOFlowControl() {};
    virtual ~IOFlowControl() {};
};

#endif
