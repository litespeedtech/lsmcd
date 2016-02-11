//config.h
// $Id: config.h,v 2.5 2001/07/09 14:39:32 gwang Exp $

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <util/hashstringmap.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

class KeyValPair
{
    AutoStr m_sKey;
    AutoStr m_sVal;

    void operator=(const KeyValPair &rhs);
public:
    KeyValPair()  {}
    KeyValPair(const KeyValPair &rhs)
        : m_sKey(rhs.m_sKey)
        , m_sVal(rhs.m_sVal)
    {}

    ~KeyValPair() {}

    void setValue(const char *v)   {   m_sVal = v;             }
    bool setValue(const char *pKey, const char *pValue);

    const char *getKey() const     {   return m_sKey.c_str();  }
    const char *getValue() const   {   return m_sVal.c_str();  }
};

class Config
{
private:
    int  m_iLineCount ;

protected:
    AutoStr     m_sDelim;
    AutoStr     m_sFileName ;            // Name of configuration file
    FILE       *m_pFileHandle;
    AutoStr     m_sError ;               // Error Message
    HashStringMap<KeyValPair *>
    m_mapConfig;  // map to store pares of key and value
    void trimSpace(char *sLine);
    bool isValidline(const char *sLine);
    bool loadConfig();

public:
    Config() ;
    virtual ~Config() ;
    bool loadConfig(const char *sFileName, const char *szDelim);
    const char *getFileName() const  {  return m_sFileName.c_str(); }
    const char *getErrorMessage() const;     //
    bool        setConfig(const char *sKey, const char *sValue);
    bool        setConfig(const AutoStr &sKey, const AutoStr &sValue);
    const char *getConfig(const char *pKey) const;
    bool        close();
    bool        open();
    bool        open(const char *szFileName);
    bool        read();
    bool        isFinish();

    virtual bool onConfigError(const AutoStr &sError);
    virtual bool onOption(const char *pchKey, const char *pchValue);      //
};




#endif // __CONFIG_H__

