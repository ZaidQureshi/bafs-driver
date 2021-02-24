#ifndef __BAFS_CORE_IOCTL_H__
#define __BAFS_CORE_IOCTL_H__

#include "bafs_common.h"

#define BAFS_CORE_IOCTL 0x80


struct BAFS_CORE_IOC_REG_MEM_PARAMS {
    //in
    unsigned long  size;
    enum LOC       loc;
    //out
    bafs_mem_hnd_t handle;

};

#define BAFS_CORE_IOC_REG_MEM _IOWR(BAFS_CORE_IOCTL, 1, struct BAFS_CORE_IOC_REG_MEM_PARAMS)

#define MAX_NAME_LEN 20
typedef char ctrl_name[MAX_NAME_LEN];

struct BAFS_CORE_IOC_CREATE_GROUP_PARAMS {
    //in
    unsigned long n_ctrls;
    ctrl_name*    ctrls;
    //out
    char*         group_name;

};

#define BAFS_CORE_IOC_CREATE_GROUP _IOWR(BAFS_CORE_IOCTL, 2, struct BAFS_CORE_IOC_CREATE_GROUP_PARAMS)

struct BAFS_CORE_IOC_DELETE_GROUP_PARAMS {
    //in
    char* group_name;

};

#define BAFS_CORE_IOC_DELETE_GROUP _IOWR(BAFS_CORE_IOCTL, 3, struct BAFS_CORE_IOC_DELETE_GROUP_PARAMS)

#endif                          // __BAFS_CORE_IOCTL_H__
