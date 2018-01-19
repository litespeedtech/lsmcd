/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include <util/emailsender.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

EmailSender::EmailSender()
{
}
EmailSender::~EmailSender()
{
}


static int callShell(const char *command)
{
    int pid;

    if (command == NULL)
        return 1;
    pid = fork();
    if (pid == -1)
        return -1;
    if (pid == 0)
    {
        char *argv[4];
        argv[0] = (char *)"sh";
        argv[1] = (char *)"-c";
        argv[2] = (char *)command;
        argv[3] = 0;
        execve("/bin/sh", argv, NULL);
        exit(0);
    }
    //return 0;

    int status;
    //cout << "waiting pid:" << pid << endl;
    do
    {
        if (waitpid(pid, &status, 0) == -1)
        {
            //cout << "waitpid() return -1, errno = " << errno << endl;
            if (errno != EINTR)
                return -1;
        }
        else
        {
            //cout << "status = " << status << endl;
            return status;
        }
    }
    while (1);

}


int EmailSender::send(const char *pSubject, const char *to,
                      const char *content, const char *cc,
                      const char *bcc)
{
    char achFileName[50] = "/tmp/m-XXXXXX";
    int fd = mkstemp(achFileName);
    if (fd == -1)
        return -1;
    int ret = write(fd, content, strlen(content));
    close(fd);
    if (-1 == ret)
    {
        return ret;
    }
    ret = sendFile(pSubject, to, achFileName, cc, bcc);
    ::unlink(achFileName);
    return ret;
}

int EmailSender::sendFile(const char *pSubject, const char *to,
                          const char *pFileName, const char *cc ,
                          const char *bcc)
{
    static const char *pMailCmds[5] =
    {
        "/usr/bin/mailx", "/bin/mailx", "/usr/bin/mail",
        "/bin/mail", "mailx"
    };
    char achCmd[2048];
    if ((!pSubject) || (!to) || (!pFileName))
    {
        errno = EINVAL;
        return -1;
    }
    const char *pMailCmd;
    int i;
    for (i = 0 ; i < 4; ++i)
    {
        if (access(pMailCmds[i], X_OK) == 0)
            break;
    }
    pMailCmd = pMailCmds[i];
    if (cc == NULL)
        cc = "";
    if (bcc == NULL)
        bcc = "";
    snprintf(achCmd, sizeof(achCmd),
             "%s -b '%s' -c '%s' -s '%s' %s < %s",
             pMailCmd, bcc, cc, pSubject, to, pFileName);
    return callShell(achCmd);
}

