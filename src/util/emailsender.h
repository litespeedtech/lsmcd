/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef EMAILSENDER_H
#define EMAILSENDER_H


/**
  *@author George Wang
  */

#include <stddef.h>

class EmailSender
{
public:
    EmailSender();
    ~EmailSender();
    static int sendFile(const char *pSubject, const char *to,
                        const char *pFileName, const char *cc = NULL,
                        const char *bcc = NULL);
    static int send(const char *pSubject, const char *to,
                    const char *content, const char *cc = NULL,
                    const char *bcc = NULL);
};

#endif
