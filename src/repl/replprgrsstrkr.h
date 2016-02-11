#ifndef REPLTMSLOT_H
#define REPLTMSLOT_H
#include "lsdef.h"
#include "repldef.h"
#include <util/dlinkqueue.h>
#include <util/hashstringmap.h>
#include <util/stringlist.h>
#include <util/autobuf.h>
#include <time.h>

enum
{
    FULL_REPL = 0,
    BULK_REPL = 1
};


class ReplProgressTrker
{
public:
    typedef THash< AutoBuf* > Cont2TmSlotMap;
    ReplProgressTrker();
    ~ReplProgressTrker();
    void setReplDone(int contId);
    
    void addBReplTmSlot(int contId, const AutoBuf &rAutoBuf);
    void addBReplTmSlot(int contId, const char *pBuf, int tmCnt);
    
    AutoBuf *getBReplTmSlot(int contId);
    int  getBReplTmRange(int contId, uint32_t &currTm, uint32_t &endTm);
    
    bool advBReplTmSlot(int contId, uint32_t tm);
    int  trackNextCont();
    
    bool isAllReplDone() const;
    bool isReplDone(int contId) const;
    int  printKeys();
        
private:    
    AutoBuf     *get (int id) const;
    bool        add (int id, AutoBuf* pAutoBuf);
    AutoBuf     *remove (int id);
private:
    Cont2TmSlotMap      m_cont2TmSlotMap;        
};


#endif
