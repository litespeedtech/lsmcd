#ifndef REPLSHMTIDCONTAINER_H
#define REPLSHMTIDCONTAINER_H

#include "lsdef.h"
#include <util/stringlist.h>
#include <repl/replicable.h>
#include <repl/replcontainer.h>
#include <util/linkedobj.h>

class ReplShmTidContainer : public ReplContainer
{
public:
    explicit ReplShmTidContainer (int id, 
                                  ReplicableAllocator * pAllocator );

    virtual ~ReplShmTidContainer();

    virtual int addAndUpdateData (uint8_t * pData, int dataLen, int iUpdate = 1);
    virtual int verifyObj( char * pBuf, int bufLen );
    virtual int purgeExpired();

    void clear()                            {}
    virtual int size()                      {}
private:
    ReplShmTidContainer ( const ReplShmTidContainer& other );
    virtual ReplShmTidContainer& operator= ( const ReplShmTidContainer& other );
    virtual bool operator== ( const ReplShmTidContainer& other ) const;
};

#endif 
