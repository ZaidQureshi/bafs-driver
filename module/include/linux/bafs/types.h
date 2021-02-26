#ifndef _LINUX_BAFS_TYPES_H_
#define _LINUX_BAFS_TYPES_H_

#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>


#include <nv-p2p.h>

#include <linux/bafs.h>
#include <linux/bafs/util.h>

#define PCI_CLASS_NVME      0x010802
#define PCI_CLASS_NVME_MASK 0xffffff

extern const struct pci_device_id pci_dev_id_table[];

struct bafs_core_ctx {
    spinlock_t       lock;
    struct xarray    bafs_mem_xa;
    struct list_head mem_list;
    struct kref      ref;
};


struct bafs_group {
    spinlock_t         lock;
    struct kref        ref;
    bafs_group_hnd_t   group_id;
    int                minor;
    struct cdev        cdev;
    struct device*     device;
    struct device*     core_dev;
    struct bafs_ctrl** ctrls;
    unsigned int       n_ctrls;
};

static inline void bafs_get_group(struct bafs_group* group) {
    BAFS_GROUP_DEBUG("In bafs_get_group: %u \t kref_bef: %u\n", group->group_id, kref_read(&group->ref));
    kref_get(&group->ref);
    BAFS_GROUP_DEBUG("In bafs_get_group: %u \t kref_bef: %u\n", group->group_id, kref_read(&group->ref));


}

static inline void bafs_put_group(struct bafs_group* group, void (*release)(struct kref *kref)) {
    BAFS_GROUP_DEBUG("In bafs_put_group: %u \t kref_bef: %u\n", group->group_id, kref_read(&group->ref));
    kref_put(&group->ref, release);
    BAFS_GROUP_DEBUG("In bafs_put_group: %u \t kref_aft: %u\n", group->group_id, kref_read(&group->ref));

}


struct bafs_ctrl {

    spinlock_t       lock;
    struct device*   dev;
    struct pci_dev*  pdev;
    struct cdev      cdev;
    struct device*   device;
    int              minor;
    int              ctrl_id;
    struct list_head group_list;
    struct rcu_head  rh;
    struct kref      ref;
    struct device* core_dev;
};



static inline struct device* bafs_get_ctrl(struct bafs_ctrl* ctrl) {
    struct device* dev;
    dev = get_device(&ctrl->pdev->dev);
    BAFS_CTRL_DEBUG("In bafs_get_ctrl: %u \t kref_bef: %u\n", ctrl->ctrl_id, kref_read(&ctrl->ref));
    kref_get(&ctrl->ref);
    BAFS_CTRL_DEBUG("In bafs_get_ctrl: %u \t kref_bef: %u\n", ctrl->ctrl_id, kref_read(&ctrl->ref));
    return dev;

}


enum STATE {
    STALE,
    LIVE,
    DEAD
};

struct bafs_mem {
    struct bafs_core_ctx*    ctx;
    spinlock_t               lock;
    struct rcu_head          rh;
    struct list_head         mem_list;
    struct list_head         dma_list;
    struct kref              ref;
    bafs_mem_hnd_t           mem_id;
    unsigned                 loc;
    enum STATE               state;
    unsigned long            vaddr;
    unsigned long            size;
    unsigned long            page_size;
    unsigned long            page_shift;
    unsigned long            page_mask;
    unsigned long            n_pages;
    nvidia_p2p_page_table_t* cuda_page_table;
    struct page**            cpu_page_table;

};

struct bafs_mem_dma {
    spinlock_t                lock;
    struct rcu_head           rh;
    struct list_head          dma_list;
    struct device*            dev;
    struct bafs_mem*          mem;
    struct bafs_ctrl*         ctrl;
    nvidia_p2p_dma_mapping_t* cuda_mapping;
    unsigned long             n_addrs;
    unsigned long *           addrs;
    unsigned                  map_gran;

};



#endif                          // __BAFS_TYPES_H__
