#ifndef __BAFS_CTRL_H__
#define __BAFS_CTRL_H__

#define BAFS_CTRL_DEVICE_NAME "bafs_ctrl%d"
#define BAFS_CTRL_CLASS_NAME  "bafs_ctrl"

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

static const struct file_operations bafs_ctrl_fops = {

    .owner     = THIS_MODULE,
//    .mmap    = bafs_core_mmap,
//    .release = bafs_core_release,

};


static inline void bafs_get_ctrl(struct bafs_ctrl* ctrl) {

    get_device(ctrl->device);

}

static inline void bafs_put_ctrl(struct bafs_ctrl* ctrl) {

    put_device(ctrl->device);

}


#endif                          // __BAFS_CTRL_H__
