/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef FDPASS_H
#define FDPASS_H


/**
  *@author George Wang
  */

class FDPass
{
public:
    FDPass();
    ~FDPass();
    static int read_fd(int fd, void *ptr, int nbytes, int *recvfd);
    static int write_fd(int fd, const void *ptr, int nbytes, int sendfd);
    static int send_fd(int fd, void *ptr, int nbytes, int fdSend);
    static int recv_fd(int fd, void *ptr, int nbytes, int *recvfd);

};

#endif
