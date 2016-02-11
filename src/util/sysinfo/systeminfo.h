/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H


/**
  *@author George Wang
  */

class SystemInfo
{
    static int s_iPageSize;

public:
    SystemInfo();
    ~SystemInfo();
    static unsigned long long maxOpenFile(unsigned long long max);
    static int getPageSize();
};

#endif
