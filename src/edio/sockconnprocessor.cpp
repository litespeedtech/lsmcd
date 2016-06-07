/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */

#include <lsdef.h>

#include "sockconnprocessor.h"
#include "socketlistener.h"

#include <socket/coresocket.h>


int SockConnProcessor::setName(const char* pName)
{
    if ( getListener() == NULL )
        return LS_FAIL;
    getListener()->setName( pName );
    return LS_OK;
}


const char* SockConnProcessor::getName() const
{
    if ( getListener() == NULL )
        return NULL;
    return getListener()->getName();
}


int SockConnProcessor::setBinding( unsigned int b )
{
    if ( getListener() == NULL )
        return LS_FAIL;
    getListener()->setBinding( b );
    return LS_OK;
}


unsigned int SockConnProcessor::getBinding() const
{
    if ( getListener() == NULL )
        return LS_OK;
    return getListener()->getBinding();
}


void SockConnProcessor::setAddrStr(const char* pAddr)
{
    if ( getListener() == NULL )
        return;
    getListener()->setAddrStr(pAddr);
}


const char* SockConnProcessor::getAddrStr() const
{
    if ( getListener() == NULL )
        return NULL;
    return getListener()->getAddrStr();
}



void SockConnProcessor::updateStats(int iCount)
{
    return;
}


int SockConnProcessor::startListening( const GSockAddr &addr, int &fd )
{
    return CoreSocket::listen( addr, -1, &fd, 0, 0, 0 );
}

