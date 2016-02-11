#include "replprgrsstrkr.h"
#include <string.h>
#include <log4cxx/logger.h>

const char * getIpOfAddr(const char *pAddr, AutoStr &str)
{
    if (pAddr == NULL)
        return NULL;

    const char *pAddr2 = strchr(pAddr, ':');
    if (pAddr2 == NULL)
        return pAddr;
    else
    {
        str.setStr(pAddr, pAddr2 - pAddr);
        return str.c_str();
    }
}

bool isEqualIpOfAddr(const char *pAddr1, const char *pAddr2)
{
    if (pAddr1 == NULL && pAddr2 == NULL)
        return true;
    else if (pAddr1 == NULL && pAddr2 != NULL)
        return false;
    else if (pAddr1!= NULL && pAddr2 == NULL)
        return false;
    else
    {
        AutoStr str1, str2;
        return !strcmp( getIpOfAddr(pAddr1, str1), getIpOfAddr(pAddr2, str2) );
    }
    
    return false;
}

IntDArr::~IntDArr()
{
    m_Buf.clear();
}

int IntDArr::add(int val)
{
    m_Buf.append((const char*)&val, sizeof(int));
    return size();
}

int & IntDArr::operator [](int idx) const
{
    return  *(int *)(m_Buf.begin() + sizeof(int) * idx);
}

int IntDArr::get(int idx, int &ret) const 
{
    if (size() < idx +1 )
        return LS_FAIL;
    ret = *(int *)(m_Buf.begin() + sizeof(int) * idx);
    return LS_OK;
}

void IntDArr::set(int idx, int val)
{
    *(int *)(m_Buf.begin() + sizeof(int) * idx) = val;
}

int IntDArr::popFront(int &ret)
{
    if (size() == 0)
        return LS_FAIL;
    ret = *(int *)m_Buf.begin();    
    m_Buf.pop_front(sizeof(int));
    return LS_OK;
}

int IntDArr::popBack(int &ret)
{
    if (size() == 0)
        return LS_FAIL;
    ret = *(int *)(m_Buf.end() - sizeof(int));    
    m_Buf.pop_end(sizeof(int));
    return LS_OK;
}

IntDArr & IntDArr::operator=(const IntDArr& rhs)
{
    m_Buf.clear();
    int size = rhs.m_Buf.size();
    m_Buf.resize(size);
    memcpy(m_Buf.begin(), rhs.m_Buf.begin(), size);
    return *this;
}

int IntDArr::size() const    
{
    return m_Buf.size() / sizeof(int);
}
    
void IntDArr::DEBUG()
{
    int v;
    int i = 0;
    while (i < size())
    {
        get(i, v);
        LS_DBG_M("DEBUG arr[%d]:%d", i, v);
        i++;
    }
}


hash_key_t repl_int_hash ( const void * p )
{
    return ( int ) (long)p;
}

int  repl_int_comp ( const void * pVal1, const void * pVal2 )
{
    return ( int ) ( long ) pVal1 - ( int ) ( long ) pVal2;
}

Int2IntMap::Int2IntMap()
    : m_key2ValMap(10, repl_int_hash, repl_int_comp )
{
}

Int2IntMap::Int2IntMap(const char *pBuf, int len)
    : m_key2ValMap(10, repl_int_hash, repl_int_comp )
{
    deserialze(pBuf, len);
}


Int2IntMap::~Int2IntMap()
{
    m_key2ValMap.release_objects();
}

int Int2IntMap::size() const
{
    return m_key2ValMap.size();
}

int Int2IntMap::getVal(int key) const
{
    Int2IntMap_t::iterator it = m_key2ValMap.find ( ( void* )( long )key );
    if ( it == m_key2ValMap.end() )
        return -1;
    return *(it.second()); 
    
}

void Int2IntMap::setVal(int key, int val)
{
    Int2IntMap_t::iterator it = m_key2ValMap.find ( ( void* )( long )key );
    assert(it != m_key2ValMap.end());
    *(it.second()) = val;
}


bool Int2IntMap::add(int key, int val)
{
    Int2IntMap_t::iterator it = m_key2ValMap.find ( ( void* )( long )key );
    if ( it != m_key2ValMap.end() )
    {
        LS_DBG_M("BReplStartTmSlot2::addContCnt found existing container id:%d", key);
        return false;
    }
    
    m_key2ValMap.insert( ( void* )( long )key, new int(val) );
    return true;        
}

//return true if found
bool Int2IntMap::addSetVal(int key, int val)
{
    Int2IntMap_t::iterator it = m_key2ValMap.find ( ( void* )( long )key );
    if ( it != m_key2ValMap.end() )
    {
        *(it.second()) = val;
        return true;
    }
    else
    {
        m_key2ValMap.insert( ( void* )( long )key, new int(val) );
        return false;
    }
}


int Int2IntMap::for_each( for_each_fn2 func, void *pUData1, void *pUData2) const
{
    Int2IntMap_t::iterator it;
    Int2IntMap_t *pInt2IntMap = (Int2IntMap_t *)&m_key2ValMap;
    for (it = pInt2IntMap->begin(); it != pInt2IntMap->end(); it = pInt2IntMap->next(it))
    {
        IntDArr arr;
        int  key = long(it.first());
        int  val = *(int*)it.second();
        arr.add(key);
        arr.add(val);        
        func( (void*)&arr, pUData1, pUData2);
    }
    return LS_OK;
}


bool Int2IntMap::isEqual( const Int2IntMap *rhs) const
{
    if (size() != rhs->size())
        return false;
    Int2IntMap_t::const_iterator it;
    Int2IntMap_t *pInt2IntMap = (Int2IntMap_t *)&m_key2ValMap;
    int key, val;
    for (it = pInt2IntMap->begin(); it != pInt2IntMap->end(); it = pInt2IntMap->next(it))
    {
        key = long(it.first());
        val = *(int*)it.second();
        if ( rhs->getVal(key) != val)
            return false;
    }
    return true;
}

int Int2IntMap::serialze(AutoBuf &rAutoBuf) const
{
    int sz0 = rAutoBuf.size();
    Int2IntMap_t::const_iterator it;
    Int2IntMap_t *pInt2IntMap = (Int2IntMap_t *)&m_key2ValMap;
    for (it = pInt2IntMap->begin(); it != pInt2IntMap->end(); it = pInt2IntMap->next(it))
    {
        int  key = long(it.first());
        int  val = *(int*)it.second();
        rAutoBuf.append((const char *)&key, sizeof(int));
        rAutoBuf.append((const char *)&val, sizeof(int));
    }
    return rAutoBuf.size() - sz0;
}

int Int2IntMap::deserialze(const char* pBuf, int len)
{
    const char *ptr;
    int cnt = len / sizeof(int);
    for (int i=0; i < cnt; i+=2)
    {
        ptr = pBuf + sizeof(int) * i;
        add(*(int*)ptr, *(int*)(ptr + sizeof(int)));
    }
    return cnt;
}

int assignedFn(void * cbData, void *pUData1, void *pUData2)
{
    IntDArr *pIntDArr = (IntDArr*)cbData;
    Int2IntMap *pInt2IntMap = (Int2IntMap*)pUData1;
    int key, val;
    pIntDArr->get(0, key);
    pIntDArr->get(1, val);
    pInt2IntMap->addSetVal(key, val);
    return LS_OK;
}

int Int2IntMap::assignTo(Int2IntMap *rhs) const
{
    for_each(assignedFn, (void*)rhs, NULL);
    return LS_OK;
}

void Int2IntMap::DEBUG() const
{
    Int2IntMap_t::const_iterator it;
    Int2IntMap_t *pInt2IntMap = (Int2IntMap_t *)&m_key2ValMap;
    for (it = pInt2IntMap->begin(); it != pInt2IntMap->end(); it = pInt2IntMap->next(it))
    {
        int  key = long(it.first());
        int  val = *(int*)it.second();
        LS_DBG_M("(key:val)=(%d:%d)", key ,val);
    }    
}

Int2IntArrMap::Int2IntArrMap()
    : m_key2ArrMap(1000, repl_int_hash, repl_int_comp )
{
}
Int2IntArrMap::~Int2IntArrMap()
{
    m_key2ArrMap.release_objects();
}
int  Int2IntArrMap::size() const
{
    return m_key2ArrMap.size();
}

IntDArr * Int2IntArrMap::getVal(int key) const
{
    Int2IntArrMap_t::iterator it = m_key2ArrMap.find ( ( void* )( long )key );
    if ( it == m_key2ArrMap.end() )
        return NULL;
    return it.second();
}

void Int2IntArrMap::setVal(int key, const IntDArr *val)
{
    Int2IntArrMap_t::iterator it = m_key2ArrMap.find ( ( void* )( long )key );
    assert ( it != m_key2ArrMap.end() );
    *(it.second()) = *val;    
}

bool Int2IntArrMap::addKey(int key, IntDArr *val)
{
    Int2IntArrMap_t::iterator it = m_key2ArrMap.find ( ( void* )( long )key );
    if ( it != m_key2ArrMap.end() )
    {
        LS_DBG_M("BReplStartTmSlot2::addContCnt found existing container id:%d", key);
        return false;
    }
    
    m_key2ArrMap.insert( ( void* )( long )key, val );
    return true;      
}

IntDArr *Int2IntArrMap::remove(int key)
{
    Int2IntArrMap_t::iterator it = m_key2ArrMap.find ( ( void* )( long )key );
    if ( it != m_key2ArrMap.end() )
    {
        IntDArr * pArr = it.second(); 
        m_key2ArrMap.erase(it);
        return pArr;
    }
    return NULL;
}

int Int2IntArrMap::for_each( for_each_fn2 func, void *pUData)
{
    int key;
    IntDArr *pArr;
    Int2IntArrMap_t::iterator it;
    for (it = m_key2ArrMap.begin(); it != m_key2ArrMap.end(); it = m_key2ArrMap.next(it))
    {
        key     = long(it.first());
        pArr    = it.second();
        func( (void *)&key, (void *)pArr, pUData);
    }
    return LS_OK;
}

void Int2IntArrMap::DEBUG()
{
    Int2IntArrMap_t::iterator it;
    for (it = m_key2ArrMap.begin(); it != m_key2ArrMap.end(); it = m_key2ArrMap.next(it))
    {
        int  key = long(it.first());
        LS_DBG_M("DEBUG key:%d", key);
        IntDArr  *val = it.second();
        val->DEBUG();
    }    
}