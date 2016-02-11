/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#ifndef RLIMITS_H
#define RLIMITS_H


/**
  *@author George Wang
  */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

class RLimits
{
#if defined(RLIMIT_AS) || defined(RLIMIT_DATA) || defined(RLIMIT_VMEM)
    struct rlimit   m_data;
#endif
#if defined(RLIMIT_NPROC)
    struct rlimit   m_nproc;
#endif

#if defined(RLIMIT_CPU)
    struct rlimit   m_cpu;
#endif

public:
    RLimits();
    ~RLimits();
    int apply() const;
    int applyMemoryLimit() const;
    int applyProcLimit() const;
    void setDataLimit(rlim_t cur, rlim_t max);
    void setProcLimit(rlim_t cur, rlim_t max);
#if defined(RLIMIT_NPROC)
    struct rlimit *getProcLimit() {    return &m_nproc;    }
#endif
    void setCPULimit(rlim_t cur, rlim_t max);
    void reset();
};

#endif
