#ifndef __BAFS_TYPES_H__
#define __BAFS_TYPES_H__


#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>



#define PCI_CLASS_NVME      0x010802
#define PCI_CLASS_NVME_MASK 0xffffff

static const struct pci_device_id pci_dev_id_table[] = {

    { PCI_DEVICE_CLASS(PCI_CLASS_NVME, PCI_CLASS_NVME_MASK) },
    {0,}

};
MODULE_DEVICE_TABLE(pci, pci_dev_id_table);


struct bafs_group {
    spinlock_t lock;
    struct list_head group_list;
    struct kref ref;
};

struct bafs_ctrl {

    spinlock_t      lock;
    struct device*  dev;
    struct pci_dev* pdev;
    struct cdev     cdev;
    struct device*  device;
    int             minor;
    int             ctrl_id;
    struct list_head group_list;
    struct rcu_head          rh;
    struct bafs_group* group;
    struct kref ref;
};



static inline struct device* bafs_get_ctrl(struct bafs_ctrl* ctrl) {
    struct device* dev;
    dev = get_device(&ctrl->pdev->dev);
    kref_get(&ctrl->ref);
    return dev;

}

static inline void bafs_put_ctrl(struct bafs_ctrl* ctrl, void (*release)(struct kref *kref)) {

    kref_put(&ctrl->ref, release);
    put_device(&ctrl->pdev->dev);

}

struct bafs_mem {

    spinlock_t               lock;
    struct rcu_head          rh;
    struct list_head         dma_list;
    struct kref              ref;
    bafs_mem_hnd_t           mem_id;
    enum LOC                 loc;
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
    struct device*  dev;
    struct bafs_mem*          mem;
    struct bafs_ctrl*         ctrl;
    nvidia_p2p_dma_mapping_t* cuda_mapping;
    unsigned long             n_addrs;
    dma_addr_t*               addrs;
    unsigned            map_gran;

};



#endif // __BAFS_TYPES_H__
