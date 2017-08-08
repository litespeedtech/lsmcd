
#include "replprgrsstrkr.h"
#include "repldef.h"
#include <string.h>
#include <log4cxx/logger.h>

extern hash_key_t repl_int_hash ( const void * p );
extern int  repl_int_comp ( const void * pVal1, const void * pVal2 );

ReplProgressTrker::ReplProgressTrker()
    : m_cont2TmSlotMap(10, repl_int_hash, repl_int_comp )
{}

ReplProgressTrker::~ReplProgressTrker()
{
    m_cont2TmSlotMap.release_objects();
}

void ReplProgressTrker::setReplDone(int contId)
{
    AutoBuf * pAutoBuf = remove(contId);
    delete pAutoBuf;
}
    

void ReplProgressTrker::addBReplTmSlot(int contId, const char *pTmBuf, int tmCnt)
{
    uint32_t  tm;
    AutoBuf * pAutoBuf = get(contId);
    assert(pAutoBuf == NULL);

    pAutoBuf = new AutoBuf();
    add(contId, pAutoBuf);
    pAutoBuf->clear();
    for (int i = 0; i < tmCnt; ++i)
    {
        tm = *(uint32_t *)(pTmBuf + sizeof(uint32_t) * i) - 1;
        pAutoBuf->append((const char*)&tm, sizeof(uint32_t));        
    }    
}
    
void ReplProgressTrker::addBReplTmSlot(int contId, const AutoBuf &rAutoBuf)
{
    addBReplTmSlot(contId, rAutoBuf.begin(), rAutoBuf.size());
}


AutoBuf *ReplProgressTrker::getBReplTmSlot(int contId)
{
    return get(contId);    
}

int ReplProgressTrker::getBReplTmRange(int contId, uint32_t &startTm, uint32_t &endTm)
{
    AutoBuf * pAutoBuf = get(contId);
    if (pAutoBuf != NULL)
    {    
        startTm = *(uint32_t *)pAutoBuf->begin();
        endTm   = *(uint32_t *)(pAutoBuf->end() - sizeof(uint32_t));  
    }
    else
    {
        startTm = 0;
        endTm   = 0;
    }
    return LS_OK;
}
 
/*
 * bulk repl: pop front elems whose time_t < tm
 * full repl: update first tm value
 * return bool  : if done
*/

bool ReplProgressTrker::advBReplTmSlot(int contId, uint32_t tm)
{
    AutoBuf * pAutoBuf = get(contId);
    assert(pAutoBuf != NULL);
    uint32_t currTm = *(uint32_t *)pAutoBuf->begin();
    int idx  = 0;
    int last = pAutoBuf->size() / sizeof(uint32_t) - 1;
    if (last == 0 && currTm == tm)
        return false;
    while ( idx <= last )
    {
        currTm  = *(uint32_t *)(pAutoBuf->begin() + sizeof(uint32_t) * idx);
        if ( currTm >= tm)
            break;
        idx++;
    }
    if ( idx <= last )
    {    
        pAutoBuf->pop_front(sizeof(uint32_t) * idx);
        return false;
    }
    setReplDone(contId);
    return true;
}


bool ReplProgressTrker::isAllReplDone() const
{
    return (m_cont2TmSlotMap.size() == 0);
}

bool ReplProgressTrker::isReplDone(int contId) const
{
    return get(contId) == NULL;
}
    

AutoBuf *ReplProgressTrker::get (int id) const
{
    Cont2TmSlotMap::iterator it = m_cont2TmSlotMap.find ( ( void* )( long )id );
    if ( it == m_cont2TmSlotMap.end() )
        return NULL;
    return it.second();    
}


AutoBuf *ReplProgressTrker::remove (int id)
{
    Cont2TmSlotMap::iterator it = m_cont2TmSlotMap.find ( ( void* )( long )id );
    if ( it == m_cont2TmSlotMap.end() )
        return NULL;
    AutoBuf *pAutoBuf = it.second();
    m_cont2TmSlotMap.erase(it);
    return pAutoBuf;
}

bool ReplProgressTrker::add (int id, AutoBuf* pAutoBuf)
{
    Cont2TmSlotMap::iterator it = m_cont2TmSlotMap.find ( ( void* )( long )id );

    if ( it != m_cont2TmSlotMap.end() )
    {
        LS_DBG_M("ReplProgressTrker::add found existing id:%d", id);
        return false;
    }
    m_cont2TmSlotMap.insert ( ( void* )( long )id, pAutoBuf );
    return true;   
}

int ReplProgressTrker::trackNextCont()
{
    if ( 0 == m_cont2TmSlotMap.size())
        return -1;
    Cont2TmSlotMap::iterator it = m_cont2TmSlotMap.begin();
    return long(it.first());
}

static int printKeysFn(GHash::iterator iter)
{
    LS_DBG_M("ReplProgressTrker container Hash key:%ld", long(iter->first()));
    return 0;
}
    
int ReplProgressTrker::printKeys()
{
    m_cont2TmSlotMap.for_each(m_cont2TmSlotMap.begin(), m_cont2TmSlotMap.end(), printKeysFn);
    return m_cont2TmSlotMap.size();
}

/*

bool ReplProgressTrker::advFReplTmSlot(int contId, uint32_t tm)
{
    AutoBuf * pAutoBuf = get(contId);
    assert(pAutoBuf != NULL);
    const char *ptr = pAutoBuf->begin();
    *(uint32_t *)ptr = tm;
    if (tm > *(uint32_t *)(ptr + sizeof(uint32_t)))
    {
        setReplDone(contId);
        return true;
    }
    return false;
}
void ReplProgressTrker::addFReplTmSlot(int contId, uint32_t startTm, uint32_t endTm)
{
    AutoBuf * pAutoBuf = get(contId);
    if (pAutoBuf == NULL)
    {
        AutoBuf * pAutoBuf = new AutoBuf();
        pAutoBuf->append((const char*)&startTm, sizeof(uint32_t));
        pAutoBuf->append((const char*)&endTm, sizeof(uint32_t));
        add(contId, pAutoBuf);
   }
    else
    {
        *(uint32_t *)pAutoBuf->begin() = startTm;
        *(uint32_t *)(pAutoBuf->begin() + sizeof(uint32_t)) = endTm;    
    }
}

int ReplProgressTrker::getFReplTmSlot(int contId, uint32_t &startTm, uint32_t &endTm)
{
    AutoBuf * pAutoBuf = get(contId);
    if (pAutoBuf == NULL)
    {
        startTm = 0;
        endTm   = 0;
        return LS_FAIL;
    }
    else
    {
        startTm = *(uint32_t *)pAutoBuf->begin();
        endTm   = *(uint32_t *)(pAutoBuf->begin() + sizeof(uint32_t)); 
        return LS_OK;
    }
}
 
*/ 


