
#include "jsonstatus.h"
//#include "replconf.h"
#include <util/autobuf.h>
#include <log4cxx/logger.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#define STATUS_FILE     "/tmp/lslbd/.replstatus"
#define STATUS_FILE_TMP "/tmp/lslbd/.replstatus.tmp"

JsonStatus::JsonStatus()
    : m_ip(NULL)
    , m_isLocal(false)
    , m_isLeader(false)
    , m_isSyncing(false)
    , m_isSync(false)
{}


JsonStatus::~JsonStatus()
{}


void JsonStatus::setNode(const char *ip, bool isLocal, bool isLeader, bool isSyncing, bool isSync
    , const NodeInfo *pNodeInfo)
{
    m_ip        = ip;
    m_isLocal   = isLeader;
    m_isLeader  =isLeader;
    m_isSyncing = isSyncing;
    m_isSync    = isSync;
    m_pNodeInfo = pNodeInfo;
}
/*
 * "Active IP1 of LB list": {
 * ...
 * }
*/
void JsonStatus::mkJson(AutoBuf &rAutoBuf) const
{
    char pBuf[128];
    snprintf(pBuf, 128, "\"%s\": {\n", m_ip);
    rAutoBuf.append(pBuf); 
    mkJsonFirstUp(rAutoBuf);
    mkJsonStatus(rAutoBuf);
    //if (m_isLocal && !m_pNodeInfo->m_fReplStat.m_isLeader)
    //    mkJsonFRepl(rAutoBuf);    
    mkJsonContainers(rAutoBuf);
    rAutoBuf.append("\n}\n");
}

//"firstUp": "yes/no", 
void JsonStatus::mkJsonFirstUp(AutoBuf &rAutoBuf) const
{
    char pBuf[128];
    snprintf(pBuf, 128, "\"firstUp\":\"%s\",\n", m_isLeader ? "Yes" : "No");
    rAutoBuf.append(pBuf);        
}

//"status": "in/out Sync",
void JsonStatus::mkJsonStatus(AutoBuf &rAutoBuf) const
{
    char pBuf[128];
    if (m_isSyncing)
        snprintf(pBuf, 128, "\"status\":\"%s\",\n", "Syncing up");
    else
        snprintf(pBuf, 128, "\"status\":\"%s\",\n", m_isSync ? "in Sync" : "out Sync");
    rAutoBuf.append(pBuf);
}

void JsonStatus::mkJsonFRepl(AutoBuf &rAutoBuf) const
{
//    char pBuf[128], pAddr[64];
    rAutoBuf.append("\"fullRepl\":{\n");
    
//     strcpy(pAddr, m_pNodeInfo->m_fReplStat.m_srcAddr.c_str());     
//     char *ptr = strchr(pAddr, ':');
//     *ptr = '\0'; 
//     snprintf(pBuf, 128, "\"destAddr\": \"%s\",\n", pAddr);
//     rAutoBuf.append(pBuf);
//     
//     snprintf(pBuf, 128, "\"startTime\": %d,\n", m_pNodeInfo->m_fReplStat.m_startTm);
//     rAutoBuf.append(pBuf);    
//     
//     snprintf(pBuf, 128, "\"endTime\": %d,\n", m_pNodeInfo->m_fReplStat.m_endTm);
//     rAutoBuf.append(pBuf);  
//     
//     snprintf(pBuf, 128, "\"dataSize\": %d\n", m_pNodeInfo->m_fReplStat.m_dataSize);
//     rAutoBuf.append(pBuf);  
//     rAutoBuf.append("},\n");  
  
}
   
int JsonStatus::writeStatusFile(AutoBuf &autoBuf) const
{
    FILE * fp;
    int size = 0;
    fp = fopen(STATUS_FILE_TMP, "w");
    if (fp != NULL)
    {
        chmod(STATUS_FILE_TMP, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
        size = fwrite(autoBuf.begin(),1 , autoBuf.size(), fp);
        fclose(fp);
        if (size == autoBuf.size()) 
        {
            rename( STATUS_FILE_TMP, STATUS_FILE);
            return LS_OK;
        }

    }
    LS_ERROR("Replication fails to refresh repl status file, error:%s", strerror(errno));
    return LS_FAIL;
}
  
void JsonStatus::removeStatusFile() const
{
    remove(STATUS_FILE);
}
