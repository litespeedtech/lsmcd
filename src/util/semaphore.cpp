/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/semaphore.h>
#include <assert.h>
#include <stdio.h>

Semaphore::Semaphore(const char *pszPath, int projId)
{
    int iSemId ;
    FILE *fp;
    assert(pszPath != NULL);
    if ((fp = fopen(pszPath, "r")) == NULL)
        fp = fopen(pszPath, "w+");
    if (fp)
    {
        fclose(fp);
        m_iSemId = ftok(pszPath, projId);
        iSemId = semget(m_iSemId, 1, IPC_CREAT);     // | IPC_EXCL
        if (iSemId != -1)
        {
            m_iSemId = iSemId ;
            //union semun semval1;
            //semval1.val = 1;
            semctl(m_iSemId, 0, SETVAL, 1);
            return;
        }
    }
    throw;
}

Semaphore::~Semaphore()
{
//    if ( m_iSemId != -1 )
//    {
//        //semctl( m_iSemId, 0, IPC_RMID, 1 );
//        m_iSemId = -1;
//    }
}
int  Semaphore::op(int sem_num, int sem_op, int sem_flag)
{
    struct sembuf semBuf;
    semBuf.sem_num = sem_num ;
    semBuf.sem_op = sem_op ;
    semBuf.sem_flg = sem_flag ;
    return op(&semBuf, 1);
}



