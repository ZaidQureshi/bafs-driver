#ifndef __BAFS_GROUP_IOCTL_H__
#define __BAFS_GROUP_IOCTL_H__

#include "bafs_common.h"

#define BAFS_GROUP_IOCTL 0x82



struct BAFS_GROUP_IOC_DMA_MAP_MEM_PARAMS {
    //in
    addr_  vaddr;
    //out
    addr_* dma_addrs;

    //inout
    unsigned long n_dma_addrs;

};

#define BAFS_GROUP_IOC_DMA_MAP_MEM _IOWR(BAFS_GROUP_IOCTL, 1, struct BAFS_GROUP_IOC_DMA_MAP_MEM_PARAMS)




#endif // __BAFS_GROUP_IOCTL_H__
