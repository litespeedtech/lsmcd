/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef XMLNODE_H
#define XMLNODE_H



#include <stdio.h>
#include <util/gpointerlist.h>

class XmlNodeImpl;
class XmlNode;
class Attr;

class XmlNodeList : public TPointerList<XmlNode>
{
public:
    explicit XmlNodeList(int n)
        : TPointerList<XmlNode>(n)
    {}
    XmlNodeList() {}
};


class AttrMap;
class XmlNode
{
private:
    XmlNode(const XmlNode &rhs);
    void operator=(const XmlNode &rhs);
    XmlNodeImpl *m_impl;

public:
    XmlNode();
    ~XmlNode();

    int init(const char *name, const char **attr);
    int addChild(const char *name, XmlNode *pChild);
    const XmlNode *getChild(const char *name) const;
    XmlNode *getChild(const char *name);
    const XmlNodeList *getChildren(const char *name) const;
    int getAllChildren(XmlNodeList &list) const;
    int getAllChildren(XmlNodeList &list);
    const char *getChildValue(const char *name) const;
    const char *getAttr(const char *name) const;
    const AttrMap *getAllAttr() const;
    const char *getName() const;
    const char *getValue() const;
    int setValue(const char *value, int len);
    XmlNode *getParent() const;
    int xmlOutput(FILE *fd, int depth) const;
};

class XmlTreeBuilder
{
public:
    XmlTreeBuilder() {};
    ~XmlTreeBuilder() {};

    XmlNode *parse(const char *fileName, char *pErrBuf, int errBufLen);
};

#endif
