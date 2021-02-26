#ifndef _LINUX_BAFS_H_
#define _LINUX_BAFS_H_

#include <linux/types.h>

typedef __u32           bafs_mem_hnd_t;
typedef __u32           bafs_group_hnd_t;

#define BAFS_MEM_CPU     0
#define BAFS_MEM_CUDA    1


/** BAFS Core IOCTL */

#define BAFS_CORE_IOCTL 0x80

struct BAFS_CORE_IOC_REG_MEM_PARAMS {
    /* in */
    __u32       size;
    __u32       loc;
    /* out */
    bafs_mem_hnd_t handle;

};

#define BAFS_CORE_IOC_REG_MEM _IOWR(BAFS_CORE_IOCTL, 1, struct BAFS_CORE_IOC_REG_MEM_PARAMS)

#define MAX_NAME_LEN 20
typedef char ctrl_name[MAX_NAME_LEN];

struct BAFS_CORE_IOC_CREATE_GROUP_PARAMS {
    /* in */
    __u32           n_ctrls;
    ctrl_name *     ctrls;
    /* out */
    char *          group_name;

};

#define BAFS_CORE_IOC_CREATE_GROUP _IOWR(BAFS_CORE_IOCTL, 2, struct BAFS_CORE_IOC_CREATE_GROUP_PARAMS)

struct BAFS_CORE_IOC_DELETE_GROUP_PARAMS {
    /* in */
    char *          group_name;

};

#define BAFS_CORE_IOC_DELETE_GROUP _IOWR(BAFS_CORE_IOCTL, 3, struct BAFS_CORE_IOC_DELETE_GROUP_PARAMS)



/* BAFS Controller IOCTL */

#define BAFS_CTRL_IOCTL 0x81


struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS {
    /* in */
    unsigned long   vaddr;
    /* out */
    unsigned long * dma_addrs;

    /* in-out */
    __u32           n_dma_addrs;

};

#define BAFS_CTRL_IOC_DMA_MAP_MEM _IOWR(BAFS_CTRL_IOCTL, 1, struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS)



/* BAFS Group IOCTL */

#define BAFS_GROUP_IOCTL 0x82

struct BAFS_GROUP_IOC_DMA_MAP_MEM_PARAMS {
    /* in */
    unsigned long   vaddr;
    /* out */
    unsigned long * dma_addrs;

    /* in-out */
    __u32           n_dma_addrs;

};

#define BAFS_GROUP_IOC_DMA_MAP_MEM _IOWR(BAFS_GROUP_IOCTL, 1, struct BAFS_GROUP_IOC_DMA_MAP_MEM_PARAMS)



#if defined(__KERNEL__)

struct vm_area_struct;

struct bafs_ctrl;
struct bafs_core_ctx;
struct bafs_mem;
struct bafs_mem_dma;

int bafs_ctrl_init(struct bafs_ctrl *);

int
bafs_ctrl_dma_map_mem(struct bafs_ctrl *, unsigned long, __u32 *, unsigned long __user *,
                      struct bafs_mem_dma **, const int);

void
bafs_ctrl_dma_unmap_mem(struct bafs_mem_dma *);

int
bafs_ctrl_mmap(struct bafs_ctrl *, struct vm_area_struct *, const unsigned long, unsigned long *);


int
pin_bafs_mem(struct vm_area_struct *, struct bafs_core_ctx *);

long
bafs_core_reg_mem(void __user *, struct bafs_core_ctx *);

void
bafs_mem_put(struct bafs_mem *);

void
unmap_dma(struct bafs_mem_dma *);

#endif

#endif
