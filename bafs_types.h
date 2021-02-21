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


struct bafs_ctrl {

    spinlock_t      lock;
    struct device*  dev;
    struct pci_dev* pdev;
    struct cdev     cdev;
    struct device*  device;
    int             minor;
    int             ctrl_id;

};

static inline void bafs_get_ctrl(struct bafs_ctrl* ctrl) {

    get_device(ctrl->device);

}

static inline void bafs_put_ctrl(struct bafs_ctrl* ctrl) {

    put_device(ctrl->device);

}

struct bafs_mem {

    spinlock_t               lock;
    struct rcu_head          rcu_head;
    struct list_head         dma_list;
    struct kref              ref_count;
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
    struct rcu_head           rcu_head;
    struct list_head          dma_list;
    struct bafs_mem*          mem;
    struct bafs_ctrl*         ctrl;
    nvidia_p2p_dma_mapping_t* cuda_mapping;
    unsigned long             n_addrs;
    dma_addr_t*               addrs;
    unsigned            map_size;

};



#endif // __BAFS_TYPES_H__
