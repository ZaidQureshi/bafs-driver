#ifndef __BAFS_PCI_H__
#define __BAFS_PCI_H__


#define PCI_CLASS_NVME      0x010802
#define PCI_CLASS_NVME_MASK 0xffffff

static const struct pci_device_id pci_dev_id_table[] = {

    { PCI_DEVICE_CLASS(PCI_CLASS_NVME, PCI_CLASS_NVME_MASK) },
    {0,}

};
MODULE_DEVICE_TABLE(pci, pci_dev_id_table);


#endif // __BAFS_PCI_H__
