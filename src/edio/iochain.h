
/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef IOCHAIN_H
#define IOCHAIN_H


/**
  *@author George Wang
  */
#include "edio/ediostream.h"
/*
class IOChain;
class InputSrc : public EdIS
{
    IOChain * m_pChain;
public:
    InputSrc()
        : m_pChain( NULL )
        {}
    void setIPChain( IOChain * pChain )
        {   m_pChain = pChain;  }

};

class OutputDst : public EdOS
{
    IOChain * m_pChain;
public:
    OutputDst()
        : m_pChain( NULL )
        {}
    void setIPChain( IOChain * pChain )
        {   m_pChain = pChain;  }

};

#include "util/loopbuf.h"
class IOChain
{
    InputSrc    * m_pSrc;
    OutputDst   * m_pDest;
    LoopBuf     m_buf;
public:
    IOChain(InputSrc* src, OutputDst* dest)
        : m_pSrc( src )
        , m_pDest( dest )
    {
        assert( m_pSrc );
        assert( m_pDest );
        m_pSrc->setIOChain( this );
        m_pDest->setIOChain( this );
    }
    ~IOChain() {};

};
*/
#endif
