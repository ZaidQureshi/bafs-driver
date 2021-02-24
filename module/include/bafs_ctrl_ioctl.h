#ifndef __BAFS_CTRL_IOCTL_H__
#define __BAFS_CTRL_IOCTL_H__

#include "bafs_common.h"

#define BAFS_CTRL_IOCTL 0x81



struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS {
    //in
    addr_  vaddr;
    //out
    addr_* dma_addrs;

    //inout
    unsigned long n_dma_addrs;

};

#define BAFS_CTRL_IOC_DMA_MAP_MEM _IOWR(BAFS_CTRL_IOCTL, 1, struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS)

#endif                          // __BAFS_CTRL_IOCTL_H__
