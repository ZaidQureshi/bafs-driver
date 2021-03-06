#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/xarray.h>
#include <linux/cdev.h>
#include <linux/pci.h>
#include <linux/mm.h>

#include <linux/bafs.h>

#include <linux/bafs/types.h>
#include <linux/bafs/util.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zaid Qureshi <zaidq2@illinois.edu>");
MODULE_DESCRIPTION("BAFS Device Driver");
MODULE_VERSION("0.1");

#define BAFS_CORE_MINOR 0

dev_t bafs_major = {0};

static int bafs_core_minor;

static struct class* bafs_core_class  = NULL;

static struct cdev bafs_core_cdev;
struct device*     bafs_core_device = NULL;

struct xarray bafs_global_ctx_xa;

static int bafs_ctrl_pci_probe(struct pci_dev* pdev, const struct pci_device_id* id)
{
    int ret = 0;
    struct bafs_ctrl * ctrl;

    BAFS_CTRL_DEBUG("Started PCI probe for PCI device: %02x:%02x.%1x\n", pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

    ret = bafs_ctrl_alloc(&ctrl, pdev, bafs_major, bafs_core_device);
    if(ret == 0) {
        BAFS_CTRL_INFO("Created controller device %s for PCI device: %02x:%02x.%1x\n",
                       dev_name(ctrl->device), pdev->bus->number, PCI_SLOT(pdev->devfn),
                       PCI_FUNC(pdev->devfn));
    }

    return ret;
}

static void bafs_ctrl_pci_remove(struct pci_dev* pdev) {

    struct bafs_ctrl* ctrl = pci_get_drvdata(pdev);

    BAFS_CTRL_DEBUG("Started PCI remove for PCI device: %02x:%02x.%1x\n", pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

    bafs_ctrl_release(ctrl);

    BAFS_CTRL_DEBUG("Finished PCI remove for PCI device: %02x:%02x.%1x\n", pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));

}




static struct pci_driver bafs_ctrl_pci_driver = {

    .name     = BAFS_CTRL_CLASS_NAME,
    .id_table = pci_dev_id_table,
    .probe    = bafs_ctrl_pci_probe,
    .remove   = bafs_ctrl_pci_remove,

};

long bafs_core_create_group(void __user* user_params)
{
    long ret;
    int i;

    struct bafs_group *                      group;
    struct BAFS_CORE_IOC_CREATE_GROUP_PARAMS params_;
    struct BAFS_CORE_IOC_CREATE_GROUP_PARAMS params;
    ctrl_name group_name;

    if (copy_from_user(&params_, user_params, sizeof(params_))) {
        ret = -EFAULT;
        BAFS_CORE_ERR("Failed to copy params from user\n");
        goto out;
    }


    params.n_ctrls = params_.n_ctrls;
    params.ctrls   = kzalloc(params.n_ctrls * sizeof(ctrl_name), GFP_KERNEL);
    if (!params.ctrls) {
        ret        = -ENOMEM;
        BAFS_CORE_ERR("Failed to allocate memory for bafs_group ctrl names\n");
        goto out;
    }

    for (i = 0; i < params.n_ctrls; i++) {
        ret = strncpy_from_user(params.ctrls[i], params_.ctrls[i], MAX_NAME_LEN);
        if (ret < 0) {
            BAFS_CORE_ERR("Failed to copy ctrl name\n");
            goto out_free_params;
        }
        else if (ret >= MAX_NAME_LEN) {
            ret = -EINVAL;
            BAFS_CORE_ERR("Failed to copy ctrl name, too long\n");
            goto out_free_params;

        }
    }

    ret = bafs_group_alloc(&group, bafs_major, bafs_core_device, params.n_ctrls, params.ctrls);
    if(ret < 0) {
        goto out_free_params;
    }


    ret = snprintf(group_name, MAX_NAME_LEN, "/dev/%s", dev_name(group->device));
    if ((ret >= MAX_NAME_LEN) || (ret == 0)) {
        BAFS_CORE_ERR("Failed to copy group name \t ret = %ld\n", ret);
        ret  = -EINVAL;

        goto out_free_group;
    }



    if (copy_to_user(params_.group_name, group_name, MAX_NAME_LEN)) {
        ret = -EFAULT;
        BAFS_CORE_ERR("Failed to copy params to user\n");
        goto out_free_group;
    }

    kfree(params.ctrls);

    return 0;
out_free_group:
    bafs_put_group(group);
out_free_params:
    kfree(params.ctrls);
out:
    return ret;
}


long bafs_core_delete_group(void __user* user_params) {

    long ret = 0;

    struct bafs_group*                       group;
    struct device*   device;
    struct BAFS_CORE_IOC_DELETE_GROUP_PARAMS params;
    ctrl_name group_name;

    BAFS_CORE_DEBUG("Started releasing group\n");

    if (copy_from_user(&params, user_params, sizeof(params))) {
        ret = -EFAULT;
        BAFS_CORE_ERR("Failed to copy params from user\n");
        goto out;
    }
    ret = strncpy_from_user(group_name, params.group_name, MAX_NAME_LEN);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to copy group name\n");
        goto out;
    }
    else if (ret == MAX_NAME_LEN) {
        ret = -EINVAL;
        BAFS_CORE_ERR("Failed to copy group name, too long\n");
        goto out;

    }

    device = device_find_child_by_name(bafs_core_device, group_name);
    if (!device) {
        ret = -EINVAL;
        goto out;
    }

    group = (struct bafs_group*) dev_get_drvdata(device);
    if (!group) {
        ret = -EFAULT;
        BAFS_CORE_ERR("Failed to find ctrl device: %s\n", group_name);
        goto out_put_device;
    }


    put_device(device);
    bafs_put_group(group);
    return ret;
out_put_device:
    put_device(device);
out:
    return ret;
}


static int bafs_core_mmap(struct file* file, struct vm_area_struct* vma) {

    int                   ret = 0;
    struct bafs_ctx* ctx;

    BAFS_CORE_DEBUG("Starting bafs_core_mmap\n");
    ctx     = (struct bafs_ctx*) file->private_data;
    if (!ctx) {
        ret = -EFAULT;
        BAFS_CORE_DEBUG("failed to get ctx\n");
        goto out;
    }

    BAFS_CORE_DEBUG("got ctx\n");

    ret = pin_bafs_mem(vma, ctx);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to mmap memory \t err = %d\n", ret);
        goto out;
    }

    return ret;


out:
    return ret;
}


static long bafs_core_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {

    long ret = 0;

    struct bafs_ctx* ctx;
    void __user*          argp = (void __user*) arg;


    BAFS_CORE_DEBUG("IOCTL called \t cmd = %u\n", cmd);

    if (_IOC_TYPE(cmd) != BAFS_CORE_IOCTL) {
        ret             = -EINVAL;
        BAFS_CORE_ERR("Invalid IOCTL commad type = %u\n", _IOC_TYPE(cmd));
        goto out;
    }

    switch (cmd) {
    case BAFS_CORE_IOC_REG_MEM:
        ctx     = (struct bafs_ctx*) file->private_data;
        if (!ctx) {
            ret = -EFAULT;
            goto out;
        }
        ret = bafs_core_reg_mem(argp, ctx);
        if (ret < 0) {
            BAFS_CORE_ERR("IOCTL to register memory failed\n");
            goto out;
        }
        break;

    case BAFS_CORE_IOC_CREATE_GROUP:
        ret = bafs_core_create_group(argp);
        if (ret < 0) {
             BAFS_CORE_ERR("IOCTL to create group failed\n");
            goto out;
        }
        break;
    case BAFS_CORE_IOC_DELETE_GROUP:
        ret = bafs_core_delete_group(argp);
        if (ret < 0) {
             BAFS_CORE_ERR("IOCTL to delete group failed\n");
            goto out;
        }
        break;
    default:
        ret                                     = -EINVAL;
        BAFS_CORE_ERR("Invalid IOCTL cmd \t cmd = %u\n", cmd);
        goto out;
        break;
    }

        ret = 0;
    return ret;
out:
    return ret;
}

struct bafs_mem* bafs_get_mem_with_ctx(const unsigned long vaddr, struct bafs_ctx* ctx) {

    struct bafs_mem* mem = NULL;
    struct bafs_mem* mem_ = NULL;
    struct bafs_mem* next = NULL;

    kref_get(&ctx->ref);


    spin_lock(&ctx->lock);
    list_for_each_entry_safe(mem_, next, &ctx->mem_list, mem_list) {
        kref_get(&mem_->ref);
        spin_lock(&mem_->lock);
        if ((mem_->vaddr == vaddr) && (mem_->state != DEAD) && (mem_->state != DEAD_CB)) {
            mem = mem_;
            spin_unlock(&mem_->lock);

            goto out_unlock_ctx;
        }
        spin_unlock(&mem_->lock);
        bafs_mem_put(mem_);

    }

out_unlock_ctx:
    spin_unlock(&ctx->lock);
    bafs_put_ctx(ctx);
//out:
    return mem;

}

struct bafs_mem* bafs_get_mem(const unsigned long vaddr) {

    pid_t tgid;
    struct pid* tgid_struct;
    struct bafs_mem* mem = NULL;
    struct bafs_mem* mem_ = NULL;
    struct bafs_mem* next = NULL;
    struct bafs_ctx* ctx;

    tgid = task_tgid_nr(current);
    tgid_struct = find_get_pid(tgid);
    if (!tgid_struct) {
        goto out;
    }

    ctx = (struct bafs_ctx*) xa_load(&bafs_global_ctx_xa, tgid);
    if ((!ctx)) {
        goto out_put_pid;
    }

    kref_get(&ctx->ref);

    if (ctx->tgid != tgid) {
        goto out_put_ctx;
    }

    spin_lock(&ctx->lock);
    list_for_each_entry_safe(mem_, next, &ctx->mem_list, mem_list) {
        kref_get(&mem_->ref);
        spin_lock(&mem_->lock);
        if ((mem_->vaddr == vaddr) && (mem_->state != DEAD) && (mem_->state != DEAD_CB)) {
            mem = mem_;
            spin_unlock(&mem_->lock);

            goto out_unlock_ctx;
        }
        spin_unlock(&mem_->lock);
        bafs_mem_put(mem_);

    }

out_unlock_ctx:
    spin_unlock(&ctx->lock);
out_put_ctx:
    bafs_put_ctx(ctx);
out_put_pid:
    put_pid(tgid_struct);
out:
    return mem;

}

static int bafs_core_open(struct inode* inode, struct file* file) {
    int                   ret;
    struct bafs_ctx* ctx;


    ctx     = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx) {
        ret = -ENOMEM;
        goto out;
    }
    ctx->tgid = task_tgid_nr(current);
    ctx->tgid_struct = find_get_pid(ctx->tgid);
    if (!ctx->tgid_struct) {
        ret = -EFAULT;
        goto out_free_ctx;
    }



    xa_init_flags(&ctx->bafs_mem_xa, XA_FLAGS_ALLOC);
    spin_lock_init(&ctx->lock);
    INIT_LIST_HEAD(&ctx->mem_list);
    kref_init(&ctx->ref);
    file->private_data = ctx;

    ret = xa_insert(&bafs_global_ctx_xa, ctx->tgid, ctx, GFP_KERNEL);
    if (ret) {
        ret = -ENOMEM;
        goto out_reset_file;
    }
    BAFS_CORE_DEBUG("Opened core and inited ctx\n");
    ret = 0;
    return ret;
out_reset_file:
    file->private_data = NULL;
//out_put_tgid:
    put_pid(ctx->tgid_struct);
out_free_ctx:
    kfree(ctx);
out:
    return ret;
}


static inline
void __bafs_core_ctx_release(struct kref* ref)
{
    struct bafs_ctx* ctx;

    ctx = container_of(ref, struct bafs_ctx, ref);

    if (ctx) {
        BAFS_CORE_DEBUG("Destroying ctx\n");
        xa_erase(&bafs_global_ctx_xa, ctx->tgid);
        put_pid(ctx->tgid_struct);
        xa_destroy(&ctx->bafs_mem_xa);
        kfree(ctx);

    }

}

void bafs_put_ctx(struct bafs_ctx * ctx)
{
    kref_put(&ctx->ref, __bafs_core_ctx_release);
}

struct bafs_ctx* bafs_get_ctx(void) {
    int                   ret;
    struct bafs_ctx* ctx = NULL;
    struct bafs_ctx* ctx_ = NULL;
    void* xa_ret;

    ctx     = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx) {
        ret = -ENOMEM;
        goto out;
    }
    ctx->tgid = task_tgid_nr(current);
    ctx->tgid_struct = find_get_pid(ctx->tgid);
    if (!ctx->tgid_struct) {
        ret = -EFAULT;
        goto out_free_ctx;
    }

    xa_init_flags(&ctx->bafs_mem_xa, XA_FLAGS_ALLOC);
    spin_lock_init(&ctx->lock);
    INIT_LIST_HEAD(&ctx->mem_list);
    kref_init(&ctx->ref);

    xa_ret =  xa_cmpxchg(&bafs_global_ctx_xa, ctx->tgid, NULL, ctx, GFP_KERNEL);
    ret = xa_err(xa_ret);

    if (ret) {
        goto out_free_mem_xa;
    }
    if (xa_ret != NULL) {
        ctx_ = (struct bafs_ctx*) xa_ret;
        kref_get(&ctx_->ref);
        put_pid(ctx->tgid_struct);
        kfree(ctx);
        ctx = ctx_;
    }

    return ctx;

out_free_mem_xa:
    xa_destroy(&ctx->bafs_mem_xa);
out_free_ctx:
    kfree(ctx);
out:
    ctx = NULL;
    return ctx;
}

static int
bafs_core_release(struct inode* inode, struct file* file)
{
    int                   ret = 0;
    struct bafs_ctx* ctx;
    struct bafs_mem*      mem;
    struct bafs_mem*      next;

    ctx = (struct bafs_ctx*) file->private_data;
    if (!ctx) {
        ret = -EINVAL;
        goto out;
    }

    spin_lock(&ctx->lock);
    list_for_each_entry_safe(mem, next, &ctx->mem_list, mem_list) {
        spin_lock(&mem->lock);
        if (mem->state == STALE) {

            list_del_init(&mem->mem_list);

            xa_erase(&ctx->bafs_mem_xa, mem->mem_id);
            BAFS_CORE_DEBUG("Deleting Stale mem registeration\n");
            spin_unlock(&mem->lock);
            kfree_rcu(mem, rh);
            kref_put(&ctx->ref, __bafs_core_ctx_release);
        }
        else {
            spin_unlock(&mem->lock);
        }


    }

    spin_unlock(&ctx->lock);

    kref_put(&ctx->ref, __bafs_core_ctx_release);
    BAFS_CORE_DEBUG("Closed core and cleaned ctx\n");
    return ret;
out:
    return ret;
}

static const struct file_operations bafs_core_fops = {

    .owner          = THIS_MODULE,
    .open           = bafs_core_open,
    .mmap           = bafs_core_mmap,
    .unlocked_ioctl = bafs_core_ioctl,
    .release        = bafs_core_release,

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
    ret = bafs_ctrl_init();
    if(ret < 0) {
        goto out_destroy_core_class;
    }

    //create group class
    ret = bafs_group_init();
    if(ret < 0) {
        goto out_ctrl_fini;
    }

    //init dev objects
    cdev_init(&bafs_core_cdev, &bafs_core_fops);
    bafs_core_cdev.owner = THIS_MODULE;




    ret = bafs_get_minor_number();
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to get minor instance id \t err = %d\n", ret);
        goto out_group_fini;

    }
    bafs_core_minor = ret;

    ret = cdev_add(&bafs_core_cdev, MKDEV(MAJOR(bafs_major), bafs_core_minor), 1);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to init core cdev \t err = %d\n", ret);
        goto out_delete_core_cdev;
    }


    //create dev
    bafs_core_device = device_create(bafs_core_class, NULL, MKDEV(MAJOR(bafs_major), bafs_core_minor), NULL, BAFS_CORE_DEVICE_NAME);
    if(IS_ERR(bafs_core_device)) {
        ret          = PTR_ERR(bafs_core_device);
        BAFS_CORE_ERR("Failed to create core device \t err = %d\n", ret);
        goto out_delete_core_cdev;
    }


    BAFS_CORE_INFO("Initialized core device: %s\n", BAFS_CORE_DEVICE_NAME);

    ret = pci_register_driver(&bafs_ctrl_pci_driver);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to register pci driver \t err = %d\n", ret);
        goto out_destroy_device;
    }

    xa_init_flags(&bafs_global_ctx_xa, XA_FLAGS_ALLOC);
    BAFS_CORE_INFO("Finished loading module\n");
    return ret;

out_destroy_device:
    device_destroy(bafs_core_class, MKDEV(MAJOR(bafs_major), bafs_core_minor));
out_delete_core_cdev:
    cdev_del(&bafs_core_cdev);
out_group_fini:
    bafs_group_fini();

out_ctrl_fini:
    bafs_ctrl_fini();

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


    device_destroy(bafs_core_class, MKDEV(MAJOR(bafs_major), bafs_core_minor));
    cdev_del(&bafs_core_cdev);
    bafs_group_fini();
    bafs_ctrl_fini();
    class_destroy(bafs_core_class);
    unregister_chrdev_region(bafs_major, BAFS_MINORS);
    xa_destroy(&bafs_global_ctx_xa);

    bafs_put_minor_number(bafs_core_minor);

    BAFS_CORE_INFO("Finished unloading module\n");

}


module_init(bafs_init);
module_exit(bafs_exit);
