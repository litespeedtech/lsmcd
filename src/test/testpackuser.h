#ifndef TESTPACKUSER_H
#define TESTPACKUSER_H

#include <replicate/replicable.h>
#include "replicate/repllog.h"
#include <stdio.h>

class TestPackUser : public Replicable
{
public:
    TestPackUser()
        : m_pUserName ( NULL )
        , m_iUserNameLen ( 0 )
        , m_Age ( 0 )
    {}
    explicit TestPackUser ( const char * pStr, char Age )
    {
        m_pUserName = strdup ( pStr );
        m_iUserNameLen = strlen ( pStr );
        m_Age = Age;
    }
    ~TestPackUser()
    {
        if ( m_pUserName )
        {
///BUG:
//            strdup() cannobe be released with delete, have to use free()
            free ( m_pUserName );
            m_pUserName = NULL;
        }

    }
    virtual int getsize ()
    {
        if ( m_pUserName == NULL )
            return -1;

        return ( sizeof ( int32_t ) * 2 + m_iUserNameLen + sizeof ( char ) );
    }
    virtual int packObj ( int32_t id, AutoBuf* pBuf )
    {
        int PackSize = sizeof ( int32_t ) * 2 + m_iUserNameLen + sizeof ( char );
        pBuf->append ( ( const char* ) &id, sizeof ( uint32_t ) );
        if(pBuf->available() < 1)
            pBuf->grow(1);
        pBuf->append ( m_Age );
        uint32_t theStrLen = m_iUserNameLen + sizeof ( uint32_t );
        pBuf->append ( ( const char* ) &theStrLen, sizeof ( uint32_t ) );
        pBuf->append ( m_pUserName );
        LS_DBG_M(  "packobj id=%d, m_pUserName=%s, m_iUserNameLen=%d, m_Age=%d, PackSize=%d\n",
                     id, m_pUserName, m_iUserNameLen, m_Age, PackSize );
        return PackSize;
    }
    virtual int packObj ( int32_t id, char * pBuf, int bufLen )
    {
        ReplPacker::detectEdian();

        int PackSize = sizeof ( int32_t ) * 2 + m_iUserNameLen + sizeof ( char ) + 1;

        if ( PackSize > bufLen )
            return -1;

        char* ptemBuf = pBuf;
        char * pEndBuf = pBuf + bufLen;
        int iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf, id );
        ptemBuf += iSavedlen;
        iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf, m_Age );
        ptemBuf += iSavedlen;
        iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf, m_pUserName, m_iUserNameLen );
        ptemBuf += iSavedlen;
        LS_DBG_M(  "packobj id=%d, m_pUserName=%s, m_iUserNameLen=%d, m_Age=%d, PackSize=%d, bufLen=%d \n",
                     id, m_pUserName, m_iUserNameLen, m_Age, PackSize, bufLen );
        return ptemBuf - pBuf;
    }
    virtual int unpackObj ( char * pBuf, int bufLen )
    {
        int32_t id, PackSize = sizeof ( int32_t ) * 2 + sizeof ( char );

        if ( PackSize >= bufLen )
            return -1;

        char* ptemBuf = pBuf;
        int temBufLen = bufLen;
        int iCopiedlen = ReplPacker::unpack ( ptemBuf, id );
        ptemBuf += iCopiedlen;
        temBufLen -= iCopiedlen;
        iCopiedlen = ReplPacker::unpack ( ptemBuf, m_Age );
        ptemBuf += iCopiedlen;
        temBufLen -= iCopiedlen;

        if ( m_pUserName != NULL )
            free ( m_pUserName );

        iCopiedlen = ReplPacker::unpack ( ptemBuf, &m_pUserName, temBufLen - sizeof ( int32_t ) );

        if ( iCopiedlen < 0 )
            return -1;

        m_iUserNameLen = strlen ( m_pUserName );
        PackSize = sizeof ( int32_t ) * 2 + m_iUserNameLen + sizeof ( char ) ;
        LS_DBG_M(  "unpackobj id=%d, m_pUserName=%s, m_iUserNameLen=%d, m_Age=%d, PackSize=%d, bufLen=%d \n",
                     id, m_pUserName, m_iUserNameLen, m_Age, PackSize, bufLen );

        return PackSize;

    }
    virtual const char * getKey ( int & keyLen )
    {
        keyLen = m_iUserNameLen;
        return m_pUserName;
    }

private:
    char * m_pUserName;
    int  m_iUserNameLen;
    char m_Age;
};
class TestPackUser1 : public Replicable
{
public:
    TestPackUser1()
        : m_pUserName ( NULL )
        , m_iUserNameLen ( 0 )
        , m_Age ( 0 )
        , m_pUserProfile ( NULL )
        , m_iUserProfileLen ( 0 )
    {}
    TestPackUser1 ( const char * pStr, char Age, char * pUserprofile, int  iUserProfileLen )
    {
        m_pUserName = strdup ( pStr );
        m_iUserNameLen = strlen ( pStr );
        m_Age = Age;
        m_pUserProfile = pUserprofile;
        m_iUserProfileLen = iUserProfileLen;
    }
    ~TestPackUser1()
    {
        if ( m_pUserName )
        {
///BUG:
//            strdup() cannobe be released with delete, have to use free()
            free ( m_pUserName );
            m_pUserName = NULL;
        }

        if ( m_pUserProfile )
        {
            delete [] m_pUserProfile;
            m_pUserProfile = NULL;
        }

    }
    virtual int getsize ()
    {
        if ( m_pUserName == NULL )
            return -1;

        return ( sizeof ( int32_t ) * 3 + m_iUserNameLen + sizeof ( char ) ) + m_iUserProfileLen;
    }
    virtual int packObj ( int32_t id, AutoBuf* pBuf )
    {
        int PackSize = sizeof ( int32_t ) * 3 + m_iUserNameLen + sizeof ( char ) + m_iUserProfileLen;
        pBuf->append ( ( const char* ) &id, sizeof ( uint32_t ) );
        if(pBuf->available() < 1)
            pBuf->grow(1);
        pBuf->append ( m_Age );
        uint32_t theStrLen = m_iUserNameLen + sizeof ( uint32_t );
        pBuf->append ( ( const char* ) &theStrLen, sizeof ( uint32_t ) );
        pBuf->append ( m_pUserName );
        //pBuf->append('\n');
        theStrLen = m_iUserProfileLen + sizeof ( uint32_t );
        pBuf->append ( ( const char* ) &theStrLen, sizeof ( uint32_t ) );
        pBuf->append ( ( const char* ) m_pUserProfile, m_iUserProfileLen );
        LS_DBG_M(  "packobj id=%d, m_pUserName=%s, m_iUserNameLen=%d, m_Age=%d, m_iUserProfileLen=%d, PackSize=%d\n",
                     id, m_pUserName, m_iUserNameLen, m_Age, m_iUserProfileLen, PackSize );
        return PackSize;
    }
    virtual int packObj ( int32_t id, char * pBuf, int bufLen )
    {
        ReplPacker::detectEdian();

        int PackSize = sizeof ( int32_t ) * 2 + m_iUserNameLen + sizeof ( char ) + 1;

        if ( PackSize > bufLen )
            return -1;

        char* ptemBuf = pBuf;
        char * pEndBuf = pBuf + bufLen;
        int iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf, id );
        ptemBuf += iSavedlen;
        iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf, m_Age );
        ptemBuf += iSavedlen;
        iSavedlen = ReplPacker::pack ( ptemBuf, pEndBuf - ptemBuf, m_pUserName, m_iUserNameLen );
        ptemBuf += iSavedlen;
        LS_DBG_M(  "packobj id=%d, m_pUserName=%s, m_iUserNameLen=%d, m_Age=%d, PackSize=%d, bufLen=%d \n",
                     id, m_pUserName, m_iUserNameLen, m_Age, PackSize, bufLen );
        return ptemBuf - pBuf;
    }
    virtual int unpackObj ( char * pBuf, int bufLen )
    {
        int32_t id, PackSize = sizeof ( int32_t ) * 3 + sizeof ( char );

        if ( PackSize >= bufLen )
            return -1;

        char* ptemBuf = pBuf;
        int temBufLen = bufLen;
        int iCopiedlen = ReplPacker::unpack ( ptemBuf, id );
        ptemBuf += iCopiedlen;
        temBufLen -= iCopiedlen;
        iCopiedlen = ReplPacker::unpack ( ptemBuf, m_Age );
        ptemBuf += iCopiedlen;
        temBufLen -= iCopiedlen;

        if ( m_pUserName != NULL )
            free ( m_pUserName );

        iCopiedlen = ReplPacker::unpack ( ptemBuf, &m_pUserName, temBufLen - sizeof ( int32_t ) );

        if ( iCopiedlen < 0 )
            return -1;

        m_iUserNameLen = strlen ( m_pUserName );
        ptemBuf += ( iCopiedlen + sizeof ( int32_t ) );
        temBufLen -= ( iCopiedlen + sizeof ( int32_t ) );

        if ( m_pUserProfile != NULL )
            delete [] m_pUserProfile;

        iCopiedlen = ReplPacker::unpack ( ptemBuf, &m_pUserProfile, temBufLen - sizeof ( int32_t ) );

        if ( iCopiedlen < 0 )
            return -1;

        m_iUserProfileLen = iCopiedlen;
        PackSize = sizeof ( int32_t ) * 3 + m_iUserNameLen + sizeof ( char ) + m_iUserProfileLen; 
        LS_DBG_M(  "unpackobj id=%d, m_pUserName=%s, m_iUserNameLen=%d, m_Age=%d, m_iUserProfileLen=%d, "
                     "PackSize=%d, bufLen=%d \n", id, m_pUserName, m_iUserNameLen, m_Age, m_iUserProfileLen,
                     PackSize, bufLen );
        LS_DBG_M(  "m_pUserProfile=%s \n", m_pUserProfile ) );
        return PackSize;

    }
    virtual const char * getKey ( int & keyLen )
    {
        keyLen = m_iUserNameLen;
        return m_pUserName;
    }

    char * getProfile()
    {
        return m_pUserProfile;
    }

private:
    char * m_pUserName;
    int  m_iUserNameLen;
    char * m_pUserProfile;
    int  m_iUserProfileLen;
    char m_Age;
};
class TestPackString : public Replicable
{
public:
    TestPackString()
        : m_pStr ( NULL )
        , m_iStrLen ( 0 )
    {}
    explicit TestPackString ( const char * pStr )
    {
        m_pStr = strdup ( pStr );
        m_iStrLen = strlen ( pStr );
    }
    ~TestPackString()
    {
        if ( m_pStr )
        {
            delete []m_pStr;
            m_pStr = NULL;
        }
    }
    virtual int getsize ()
    {
        if ( m_pStr == NULL )
            return -1;

        return ( sizeof ( int32_t )*2 + m_iStrLen );
    }
    virtual int packObj ( int32_t id, AutoBuf* pBuf )
    {
        int PackSize = sizeof ( int32_t )*2 + m_iStrLen;
        pBuf->append ( ( const char* ) &id, sizeof ( uint32_t ) );
        uint32_t theStrLen = m_iStrLen + sizeof ( uint32_t );
        pBuf->append ( ( const char* ) &theStrLen, sizeof ( uint32_t ) );       
        pBuf->append ( m_pStr );
        //pBuf->append('\n');
        LS_DBG_M(  "packobj id=%d, m_pStr=%s, m_iStrLen=%d, PackSize=%d \n",
                     id, m_pStr, m_iStrLen, PackSize ) );
        return PackSize;
    }
    virtual int packObj ( int32_t id, char * pBuf, int bufLen )
    {

        int PackSize = sizeof ( int32_t )*2 + m_iStrLen;
        LS_DBG_M(  "packobj id=%d, m_pStr=%s, m_iStrLen=%d, PackSize=%d, bufLen=%d \n",
                     id, m_pStr, m_iStrLen, PackSize, bufLen );

        if ( PackSize > bufLen )
            return -1;

        memset ( pBuf, 0, PackSize );
        memcpy ( pBuf, &id, sizeof ( int32_t ) );
        uint32_t theStrLen = m_iStrLen + sizeof ( uint32_t );
        memcpy ( pBuf+ sizeof ( int32_t ), &theStrLen, sizeof ( int32_t ) );
        memcpy ( pBuf + sizeof ( int32_t )*2, m_pStr, m_iStrLen );
        return PackSize;
    }
    virtual int unpackObj ( char * pBuf, int bufLen )
    {
        int32_t id, theStrLen;
        memcpy ( &id, pBuf, sizeof ( int32_t ) );
        memcpy ( &theStrLen, pBuf + sizeof ( int32_t ) , sizeof ( int32_t ) );
        if ( m_pStr != NULL )
            delete []m_pStr;
        theStrLen -= sizeof ( int32_t );
        m_pStr = new char[theStrLen + 1];

        if ( m_pStr == NULL )
            return -1;

        m_pStr[theStrLen] = 0;
        memcpy ( m_pStr, pBuf + sizeof ( int32_t )*2, theStrLen );
        m_iStrLen = strlen ( m_pStr );
        int PackSize = sizeof ( int32_t )*2 + theStrLen;
        LS_DBG_M(  "unpackobj id=%d, m_pStr=%s, m_iStrLen=%d, PackSize=%d, bufLen=%d \n",
                     id, m_pStr, m_iStrLen, PackSize, bufLen );

        return PackSize;

    }
    virtual const char * getKey ( int & keyLen )
    {
        keyLen = m_iStrLen;
        return m_pStr;
    }

private:
    char * m_pStr;
    int  m_iStrLen;
};
#endif // TESTPACKUSER_H
