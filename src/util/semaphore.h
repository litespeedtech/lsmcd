/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef SEMAPHORE_H
#define SEMAPHORE_H


/**
  *@author George Wang
  */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

class Semaphore
{
private:
    int m_iSemId;
    Semaphore(const Semaphore &);
public:
    explicit Semaphore(int id)
        : m_iSemId(id)
    {}
    Semaphore(const char *pszPath, int projId);
    ~Semaphore();
    int  getSemId() const   {   return m_iSemId ; }
    int  op(struct sembuf *sops, unsigned nsops)
    {   return  semop(m_iSemId, sops, nsops);  }
    int  op(int sem_num, int sem_op, int sem_flag);
};


#endif
