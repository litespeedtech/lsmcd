
#ifndef GZIPSTREAM_H
#define GZIPSTREAM_H

#include <util/loopbuf.h>
#include <zlib.h>

enum
{
    ZIP_DEF=1,
    ZIP_INF
};

class GzipStream
{
public:

    GzipStream(int mode, LoopBuf* pLBuf, int level = Z_DEFAULT_COMPRESSION);
    ~GzipStream();
    int pushOutGoingData(const char* pData, int dataLen);
    int popIncomingData(const char* pData, int dataLen);
    
    int compress (const char *pInBuf, int dataLen);
    int uncompress (const char *pInBuf, int dataLen);
    int initDef();
    int initInf();
    
    const char* getZipErr(int errno) const ;

private:
    int release();
    int getZipOutput(char **ptr);
   
    z_stream        m_zstrm;
    int             m_errno;
    int             m_zLevel;
    int             m_zipMode;
    LoopBuf        *m_pLoopBuf;
};



#endif
