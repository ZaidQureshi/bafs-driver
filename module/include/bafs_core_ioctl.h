#ifndef __BAFS_CORE_IOCTL_H__
#define __BAFS_CORE_IOCTL_H__

#include "bafs_common.h"

#define BAFS_CORE_IOCTL 0x80




struct BAFS_CORE_IOC_REG_MEM_PARAMS {
    //in
    u64            size;
    enum LOC       loc;
    //out
    bafs_mem_hnd_t handle;

};

#define BAFS_CORE_IOC_REG_MEM _IOWR(BAFS_CORE_IOCTL, 1, struct BAFS_CORE_IOC_REG_MEM_PARAMS)

#endif                          // __BAFS_CORE_IOCTL_H__
