#include <linux/module.h>
#include <linux/pci.h>

#include <linux/bafs/mem.h>

const struct vm_operations_struct bafs_mem_ops = {

    .close = bafs_mem_release,

};


const struct pci_device_id pci_dev_id_table[] = {

    { PCI_DEVICE_CLASS(PCI_CLASS_NVME, PCI_CLASS_NVME_MASK) },
    {0,}

};


MODULE_DEVICE_TABLE(pci, pci_dev_id_table);
