#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/xarray.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/mm.h>

#include <bafs_ctrl.h>
#include <bafs_group.h>
#include <bafs_mem.h>
#include <bafs_core_ioctl.h>
#include <bafs_ctrl_ioctl.h>
#include <bafs_util.h>
#include <bafs_ctrl_release.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zaid Qureshi <zaidq2@illinois.edu>");
MODULE_DESCRIPTION("BAFS Device Driver");
MODULE_VERSION("0.1");

#define BAFS_MINORS     (1U << MINORBITS)
#define BAFS_CORE_MINOR 0



dev_t bafs_major = {0};

static struct class* bafs_core_class  = NULL;
struct class* bafs_ctrl_class  = NULL;
struct class* bafs_group_class = NULL;


static struct cdev    bafs_core_cdev;
static struct device* bafs_core_device = NULL;

DEFINE_IDA(bafs_minor_ida);
DEFINE_IDA(bafs_ctrl_ida);
//static DEFINE_IDA(bafs_group_ida);

struct xarray bafs_mem_xa = {0};


static int bafs_ctrl_pci_probe(struct pci_dev* pdev, const struct pci_device_id* id) {

    int ret = 0;

    struct bafs_ctrl* ctrl;

    BAFS_CTRL_DEBUG("Started PCI probe for PCI device: %02x:%02x.%1x\n", pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

    ctrl    = kzalloc(sizeof(*ctrl), GFP_KERNEL);
    if (!ctrl) {
        ret = -ENOMEM;
        goto out;
    }

    ctrl->dev  = get_device(&pdev->dev);
    ctrl->pdev = pdev;

    pci_set_master(pdev);
    pci_set_drvdata(pdev, ctrl);
    pci_free_irq_vectors(pdev);
    pci_disable_msi(pdev);
    pci_disable_msix(pdev);
    dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));

    ret = pci_request_region(pdev, 0, BAFS_CTRL_CLASS_NAME);
    if (ret < 0) {
        goto out_clear_pci_drvdata;
    }

    ret = pci_enable_device(pdev);
    if (ret < 0) {
        goto out_release_pci_region;
    }




    spin_lock_init(&ctrl->lock);
    INIT_LIST_HEAD(&ctrl->group_list);




    ret = ida_simple_get(&bafs_minor_ida, 1, BAFS_MINORS, GFP_KERNEL);
    if (ret < 0) {
        BAFS_CTRL_ERR("Failed to get minor instance id \t err = %d\n", ret);
        goto out_disable_pci_device;
    }
    ctrl->minor = ret;

    ret = ida_simple_get(&bafs_ctrl_ida, 0, 0, GFP_KERNEL);
    if (ret < 0) {
        BAFS_CTRL_ERR("Failed to get ctrl instance id \t err = %d\n", ret);
        goto out_release_minor_instance;
    }
    ctrl->ctrl_id = ret;

    cdev_init(&ctrl->cdev, &bafs_ctrl_fops);
    ctrl->cdev.owner = THIS_MODULE;
    ret              = cdev_add(&ctrl->cdev, MKDEV(MAJOR(bafs_major), ctrl->minor), 1);
    if (ret < 0) {
        BAFS_CTRL_ERR("Failed to init ctrl cdev \t err = %d\n", ret);
        goto out_release_ctrl_instance;
    }

    ctrl->device = device_create(bafs_ctrl_class, bafs_core_device, MKDEV(MAJOR(bafs_major), ctrl->minor), ctrl, BAFS_CTRL_DEVICE_NAME, ctrl->ctrl_id);
    if (IS_ERR(ctrl->device)) {
        ret      = PTR_ERR(ctrl->device);
        BAFS_CORE_ERR("Failed to create ctrl device \t err = %d\n", ret);
        goto out_delete_device_cdev;
    }

    BAFS_CTRL_INFO("Created controller device %s for PCI device: %02x:%02x.%1x\n",
                   dev_name(ctrl->device), pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));



    //BAFS_CTRL_DEBUG("Finished PCI probe for PCI device: %02x:%02x.%1x\n", pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
    return ret;

out_delete_device_cdev:
    cdev_del(&ctrl->cdev);
out_release_ctrl_instance:
    ida_simple_remove(&bafs_ctrl_ida, ctrl->ctrl_id);
out_release_minor_instance:
    ida_simple_remove(&bafs_minor_ida, ctrl->minor);
out_disable_pci_device:
    pci_disable_device(pdev);
out_release_pci_region:
    pci_release_region(pdev, 0);
out_clear_pci_drvdata:
    pci_set_drvdata(pdev, NULL);
//out_clear_pci_master:
    pci_clear_master(pdev);
    put_device(&pdev->dev);
//out_delete_ctrl:
    kfree(ctrl);
out:
    return ret;

}

static void bafs_ctrl_pci_remove(struct pci_dev* pdev) {

    struct bafs_ctrl* ctrl = pci_get_drvdata(pdev);
    BAFS_CTRL_DEBUG("Started PCI remove for PCI device: %02x:%02x.%1x\n", pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
    kref_put(&ctrl->ref, __bafs_ctrl_release);


    BAFS_CTRL_DEBUG("Finished PCI remove for PCI device: %02x:%02x.%1x\n", pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

}




static struct pci_driver bafs_ctrl_pci_driver = {

    .name     = BAFS_CTRL_CLASS_NAME,
    .id_table = pci_dev_id_table,
    .probe    = bafs_ctrl_pci_probe,
    .remove   = bafs_ctrl_pci_remove,

};




static int bafs_core_mmap(struct file* file, struct vm_area_struct* vma) {

    int              ret = 0;



    ret = pin_bafs_mem(vma);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to mmap memory \t err = %d\n", ret);
        goto out;
    }

    //vma->vm_private_data = mem;

    return ret;


out:
    return ret;
}


static long bafs_core_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {

    long                   ret = 0;

    void __user* argp = (void __user*) arg;


    BAFS_CORE_DEBUG("IOCTL called \t cmd = %u\n", cmd);

    if (_IOC_TYPE(cmd) != BAFS_CORE_IOCTL) {
        ret             = -EINVAL;
        BAFS_CORE_ERR("Invalid IOCTL commad type = %u\n", _IOC_TYPE(cmd));
        goto out;
    }

    switch (cmd) {
    case BAFS_CORE_IOC_REG_MEM:
        ret = bafs_core_reg_mem(argp);
        if (ret < 0) {
            BAFS_CORE_ERR("IOCTL to register memory failed\n");
            goto out;
        }
        break;
    default:
        ret = -EINVAL;
        BAFS_CORE_ERR("Invalid IOCTL cmd \t cmd = %u\n", cmd);
        goto out;
        break;
    }

    return ret;
out:
    return ret;
}

static const struct file_operations bafs_core_fops = {

    .owner          = THIS_MODULE,
    .mmap           = bafs_core_mmap,
    .unlocked_ioctl = bafs_core_ioctl,

};

static int __init bafs_init(void) {

    int ret = 0;

    BAFS_CORE_DEBUG("Started loading module\n");

    //init major
    ret = alloc_chrdev_region(&bafs_major, 0, BAFS_MINORS, BAFS_CORE_DEVICE_NAME);
    if (ret < 0)
        goto out;
    
    //create core class
    bafs_core_class = class_create(THIS_MODULE, BAFS_CORE_CLASS_NAME);
    if (IS_ERR(bafs_core_class)) {
        ret         = PTR_ERR(bafs_core_class);
        BAFS_CORE_ERR("Failed to create core class \t err = %d\n", ret);
        goto out_unregister_core_major_region;
    }

    //create ctrl class
    bafs_ctrl_class = class_create(THIS_MODULE, BAFS_CTRL_CLASS_NAME);
    if (IS_ERR(bafs_ctrl_class)) {
        ret         = PTR_ERR(bafs_ctrl_class);
        BAFS_CORE_ERR("Failed to create ctrl class \t err = %d\n", ret);
        goto out_destroy_core_class;
    }

    //create group class
    bafs_group_class = class_create(THIS_MODULE, BAFS_GROUP_CLASS_NAME);
    if (IS_ERR(bafs_group_class)) {
        ret          = PTR_ERR(bafs_group_class);
        BAFS_CORE_ERR("Failed to create group class \t err = %d\n", ret);
        goto out_destroy_ctrl_class;
    }

    //init dev objects
    cdev_init(&bafs_core_cdev, &bafs_core_fops);
    bafs_core_cdev.owner = THIS_MODULE;
    ret                  = cdev_add(&bafs_core_cdev, MKDEV(MAJOR(bafs_major), BAFS_CORE_MINOR), 1);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to init core cdev \t err = %d\n", ret);
        goto out_destroy_group_class;
    }

    //create dev
    bafs_core_device = device_create(bafs_core_class, NULL, MKDEV(MAJOR(bafs_major), BAFS_CORE_MINOR), NULL, BAFS_CORE_DEVICE_NAME);
    if(IS_ERR(bafs_core_device)) {
        ret                                                = PTR_ERR(bafs_core_device);
        BAFS_CORE_ERR("Failed to create core device \t err = %d\n", ret);
        goto out_delete_core_cdev;
    }

    xa_init(&bafs_mem_xa);
    BAFS_CORE_INFO("Initialized core device: %s\n", BAFS_CORE_DEVICE_NAME);

    ret = pci_register_driver(&bafs_ctrl_pci_driver);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to register pci driver \t err = %d\n", ret);
        goto out_destroy_device;
    }

    BAFS_CORE_INFO("Finished loading module\n");
    return ret;

out_destroy_device:
    device_destroy(bafs_core_class, MKDEV(MAJOR(bafs_major), BAFS_CORE_MINOR));
out_delete_core_cdev:
    cdev_del(&bafs_core_cdev);
out_destroy_group_class:
    class_destroy(bafs_group_class);
out_destroy_ctrl_class:
    class_destroy(bafs_ctrl_class);
out_destroy_core_class:
    class_destroy(bafs_core_class);
out_unregister_core_major_region:
    unregister_chrdev_region(bafs_major, BAFS_MINORS);
out:
    return ret;

}

static void __exit bafs_exit(void) {

    BAFS_CORE_DEBUG("Start unloading module\n");

    pci_unregister_driver(&bafs_ctrl_pci_driver);
    xa_destroy(&bafs_mem_xa);

    device_destroy(bafs_core_class, MKDEV(MAJOR(bafs_major), BAFS_CORE_MINOR));
    cdev_del(&bafs_core_cdev);
    class_destroy(bafs_group_class);
    class_destroy(bafs_ctrl_class);
    class_destroy(bafs_core_class);
    unregister_chrdev_region(bafs_major, BAFS_MINORS);

    BAFS_CORE_INFO("Finished unloading module\n");

}


module_init(bafs_init);
module_exit(bafs_exit);
