#ifndef _REPLDEF_H
#define _REPLDEF_H
#include "lsdef.h"
#include <util/dlinkqueue.h>
#include <util/hashstringmap.h>
#include <util/stringlist.h>
#include <util/autobuf.h>
#include <time.h>

bool    isEqualIpOfAddr(const char *pAddr1, const char *pAddr2);
const char*getIpOfAddr(const char *pAddr, AutoStr &autoStr);

class IntDArr
{
public:
    IntDArr() {}
    ~IntDArr();
    int & operator [](int idx) const;
    IntDArr & operator=(const IntDArr& rhs);
    
    int  add(int val);
    int  get(int idx, int &ret) const;
    void set(int idx, int val);
    int  popFront(int &ret);
    int  popBack(int &ret);
    int  size() const;
    AutoBuf *getBuf()   {       return &m_Buf;          }
    void DEBUG();
private:
    AutoBuf m_Buf;        
};

typedef THash< int * > Int2IntMap_t;

typedef int (*for_each_fn2)(void *pUData1, void *pUData2,  void *pUData3);
class Int2IntMap
{
public:
    Int2IntMap();
    Int2IntMap(const char *pBuf, int len);    
    ~Int2IntMap();
    int  size() const;
    int  getVal(int key) const;
    void setVal(int key, int val);
    bool add(int key, int val);
    bool addSetVal(int key, int val);
    int  for_each( for_each_fn2, void *pUData1, void *pUData2) const;
    bool isEqual(const Int2IntMap *rhs) const;
    int  serialze(AutoBuf &rAutoBuf) const;
    int  deserialze(const char* pBuf, int len);
    int  assignTo(Int2IntMap *rhs) const;
    void DEBUG() const;
private:
    Int2IntMap_t m_key2ValMap;
};

typedef THash< IntDArr * > Int2IntArrMap_t;
class Int2IntArrMap
{
public:
    Int2IntArrMap();
    ~Int2IntArrMap();
    int  size() const;
    IntDArr  *getVal(int key) const;
    void setVal(int key, const IntDArr *val);
    bool addKey(int key, IntDArr *val);
    IntDArr *remove(int key);
    int  for_each( for_each_fn2, void *pUData);
    void DEBUG();
private:
    Int2IntArrMap_t m_key2ArrMap;
};

#endif
