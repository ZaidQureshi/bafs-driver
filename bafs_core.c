#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/pci.h>

#include "bafs_pci.h"
#include "bafs_ctrl.h"
#include "bafs_group.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zaid Qureshi <zaidq2@illinois.edu>");
MODULE_DESCRIPTION("BAFS Device Driver");
MODULE_VERSION("0.1");

#define BAFS_MINORS (1U << MINORBITS)

#define BAFS_CORE_DEVICE_NAME "bafs_core"
#define BAFS_CORE_CLASS_NAME  "bafs_core"

static dev_t bafs_major;

static struct class* bafs_core_class  = NULL;
static struct class* bafs_ctrl_class  = NULL;
static struct class* bafs_group_class = NULL;


static struct cdev    bafs_core_cdev;
static struct device* bafs_core_device;

static DEFINE_IDA(bafs_minor_ida);
static DEFINE_IDA(bafs_ctrl_ida);
//static DEFINE_IDA(bafs_group_ida);

static int bafs_ctrl_pci_probe(struct pci_dev* pdev, const struct pci_device_id* id) {

    int               ret;
    struct bafs_ctrl* ctrl;

    ctrl    = kzalloc(sizeof(*ctrl), GFP_KERNEL);
    if (!ctrl) {
        ret = -ENOMEM;
        goto out;
    }


    pci_set_master(pdev);
    pci_set_drvdata(pdev, ctrl);
    pci_free_irq_vectors(pdev);
    dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));

    ret = pci_request_region(pdev, 0, BAFS_CTRL_CLASS_NAME);
    if (ret < 0) {
        goto out_clear_pci_drvdata;
    }

    ret = pci_enable_device(pdev);
    if (ret < 0) {
        goto out_release_pci_region;
    }


    ctrl->dev  = get_device(&pdev->dev);
    ctrl->pdev = pdev;
    spin_lock_init(&ctrl->lock);

    ret = ida_simple_get(&bafs_minor_ida, 1, BAFS_MINORS, GFP_KERNEL);
    if (ret < 0) {
        goto out_disable_pci_device;
    }
    ctrl->minor = ret;

    ret = ida_simple_get(&bafs_ctrl_ida, 0, 0, GFP_KERNEL);
    if (ret < 0) {
        goto out_release_minor_instance;
    }
    ctrl->ctrl_id = ret;

    cdev_init(&ctrl->cdev, &bafs_ctrl_fops);
    ctrl->cdev.owner = THIS_MODULE;
    ret              = cdev_add(&ctrl->cdev, MKDEV(MAJOR(bafs_major), ctrl->minor), 1);
    if (ret < 0) {
        goto out_release_ctrl_instance;
    }

    ctrl->device = device_create(bafs_ctrl_class, bafs_core_device, MKDEV(MAJOR(bafs_major), ctrl->minor), ctrl, BAFS_CTRL_DEVICE_NAME, ctrl->ctrl_id);
    if (IS_ERR(ctrl->device)) {
        ret      = PTR_ERR(ctrl->device);
        goto out_delete_device_cdev;
    }

    bafs_get_ctrl(ctrl);

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
//out_delete_ctrl:
    kfree(ctrl);
out:
    return ret;

}

static void bafs_ctrl_pci_remove(struct pci_dev* pdev) {

    struct bafs_ctrl* ctrl = pci_get_drvdata(pdev);
    device_destroy(bafs_ctrl_class, MKDEV(MAJOR(bafs_major), ctrl->minor));
    cdev_del(&ctrl->cdev);
    ida_simple_remove(&bafs_ctrl_ida, ctrl->ctrl_id);
    ida_simple_remove(&bafs_minor_ida, ctrl->minor);
    pci_disable_device(pdev);
    pci_release_region(pdev, 0);
    pci_set_drvdata(pdev, NULL);
    pci_clear_master(pdev);
    kfree(ctrl);

}




static struct pci_driver bafs_ctrl_pci_driver = {

    .name     = BAFS_CTRL_DEVICE_NAME,
    .id_table = pci_dev_id_table,
    .probe    = bafs_ctrl_pci_probe,
    .remove   = bafs_ctrl_pci_remove,

};



static const struct file_operations bafs_core_fops = {

    .owner     = THIS_MODULE,
//    .mmap    = bafs_core_mmap,
//    .release = bafs_core_release,

};

static int __init bafs_init(void) {

    int ret = 0;

    //init major
    ret = alloc_chrdev_region(&bafs_major, 0, BAFS_MINORS, BAFS_CORE_DEVICE_NAME);
    if (ret < 0)
        goto out;

    //create core class
    bafs_core_class = class_create(THIS_MODULE, BAFS_CORE_CLASS_NAME);
    if (IS_ERR(bafs_core_class)) {
        ret         = PTR_ERR(bafs_core_class);
        goto out_unregister_core_major_region;
    }

    //create ctrl class
    bafs_ctrl_class = class_create(THIS_MODULE, BAFS_CTRL_CLASS_NAME);
    if (IS_ERR(bafs_ctrl_class)) {
        ret         = PTR_ERR(bafs_ctrl_class);
        goto out_destroy_core_class;
    }

    //create group class
    bafs_group_class = class_create(THIS_MODULE, BAFS_GROUP_CLASS_NAME);
    if (IS_ERR(bafs_group_class)) {
        ret          = PTR_ERR(bafs_group_class);
        goto out_destroy_ctrl_class;
    }

    //init dev objects
    cdev_init(&bafs_core_cdev, &bafs_core_fops);
    bafs_core_cdev.owner = THIS_MODULE;
    ret                  = cdev_add(&bafs_core_cdev, MKDEV(MAJOR(bafs_major), 0), 1);
    if (ret < 0) {
        goto out_destroy_group_class;
    }

    //create dev
    bafs_core_device = device_create(bafs_core_class, NULL, MKDEV(MAJOR(bafs_major), 0), NULL, BAFS_CORE_DEVICE_NAME);
    if(IS_ERR(bafs_core_device)) {
        ret = PTR_ERR(bafs_core_device);
        goto out_delete_core_cdev;
    }

    ret = pci_register_driver(&bafs_ctrl_pci_driver);
    if (ret < 0) {
        goto out_destroy_device;
    }

    return ret;

out_destroy_device:
    device_destroy(bafs_core_class, MKDEV(MAJOR(bafs_major), 0));
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

    pci_unregister_driver(&bafs_ctrl_pci_driver);
    device_destroy(bafs_core_class, MKDEV(MAJOR(bafs_major), 0));
    cdev_del(&bafs_core_cdev);

    class_destroy(bafs_group_class);
    class_destroy(bafs_ctrl_class);
    class_destroy(bafs_core_class);
    unregister_chrdev_region(bafs_major, BAFS_MINORS);

}


module_init(bafs_init);
module_exit(bafs_exit);
