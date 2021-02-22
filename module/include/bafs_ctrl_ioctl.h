#ifndef __BAFS_CTRL_IOCTL_H__
#define __BAFS_CTRL_IOCTL_H__

#include "bafs_common.h"

#define BAFS_CTRL_IOCTL 0x81



struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS {
    //in
    bafs_mem_hnd_t handle;
    //out
    uint64_t*      dma_addrs;

    //inout
    uint64_t       n_dma_addrs;

};

#define BAFS_CTRL_IOC_DMA_MAP_MEM _IOWR(BAFS_CTRL_IOCTL, 1, struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS)

#endif                          // __BAFS_CTRL_IOCTL_H__
