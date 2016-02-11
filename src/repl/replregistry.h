#ifndef REPLREGISTRY_H
#define REPLREGISTRY_H

#include "replcontainer.h"
#include <util/tsingleton.h>
#include <util/ghash.h>

typedef int (*for_each_fn2)(void *pUData1, void *pUData2,  void *pUData3);
typedef THash< ReplContainer* > ContMap;
class ReplRegistry : public TSingleton<ReplRegistry>
{
    friend class TSingleton<ReplRegistry>;
public:
    virtual ~ReplRegistry();
    ReplContainer * get ( int32_t id );
    ReplContainer * remove ( int32_t id );
    int add ( int32_t id, ReplContainer *pContainer );
    int size() const;
    int for_each( for_each_fn2, void *pUData1, void *pUData2);
    int getContId2CntBuf(AutoBuf *pAutoBuf);
    int purgeAllExpired();
    const char *getContNameById(int Id);
    
    int  packLruHashTmRange(AutoBuf &rAutoBuf);
    int  getContsLruMinMaxTm(uint32_t &minTm, uint32_t &maxTm);
    time_t getAllContNextTm(time_t tm); 
    int  fillContsAllTm(uint32_t startTm, uint32_t endTm, AutoBuf &autoBuf);
    
    int  getContNameMD5Lst(AutoBuf &rAutoBuf);
    int  cmpContNameMD5Lst(const char *pLocalMD5Lst, int len1, const char *pPeerMD5Lst, int len2) const;
   
    int  calRTReplHash(time_t startTm, time_t endTm, AutoBuf &rAutoBuf);
private:
    ReplRegistry();
    ReplRegistry ( const ReplRegistry& other );
    virtual ReplRegistry& operator= ( const ReplRegistry& other );
private:
    ContMap m_pConMap;
};

#endif // REPLREGISTRY_H
