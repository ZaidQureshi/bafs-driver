#include <linux/cdev.h>
#include <asm/uaccess.h>

#include <linux/bafs.h>

#include <linux/bafs/types.h>
#include <linux/bafs/util.h>

static DEFINE_IDA(bafs_group_ida);
static struct class *   bafs_group_class = NULL;

static
void __bafs_group_release(struct kref* ref)
{
    int j;
    struct bafs_group * group;

    WARN_ON(ref == NULL);
    if(ref == NULL) return;

    group = container_of(ref, struct bafs_group, ref);
    BAFS_GROUP_DEBUG("Removing GROUP \t group: %p\n", group);

    spin_lock(&group->lock);
    device_destroy(bafs_group_class, MKDEV(group->major, group->minor));
    for (j = 0; j < group->n_ctrls; j++) {
        WARN_ON(group->ctrls[j] == NULL);
        if(group->ctrls[j] == NULL) continue;

        bafs_ctrl_release(group->ctrls[j]);
    }

    kfree(group->ctrls);

    put_device(group->core_dev);
    cdev_del(&group->cdev);
    ida_simple_remove(&bafs_group_ida, group->group_id);
    bafs_put_minor_number(group->minor);
    spin_unlock(&group->lock);
    BAFS_CTRL_DEBUG("Removed GROUP \t group: %p\n", group);

    kfree(group);
}

void bafs_put_group(struct bafs_group * group)
{
    BAFS_GROUP_DEBUG("In bafs_put_group: %u \t kref_bef: %u\n", group->group_id, kref_read(&group->ref));
    kref_put(&group->ref, __bafs_group_release);
    BAFS_GROUP_DEBUG("In bafs_put_group: %u \t kref_aft: %u\n", group->group_id, kref_read(&group->ref));
}



long
bafs_group_dma_map_mem(struct bafs_group* group, struct bafs_ctx* ctx, void __user* user_params)
{
    long     ret                  = 0;
    int      i                    = 0;
    uint64_t n_dma_addrs_per_ctrl = 0;

    struct bafs_mem_dma**                    dmas;

    struct BAFS_IOC_DMA_MAP_MEM_PARAMS params = {0};


    if (copy_from_user(&params, user_params, sizeof(params))) {
        ret = -EFAULT;
        BAFS_GROUP_ERR("Failed to copy params from user\n");
        goto out;
    }
    spin_lock(&group->lock);
    dmas = kzalloc(sizeof(*dmas) * group->n_ctrls, GFP_KERNEL);
    if (!dmas) {
        ret = -ENOMEM;
        BAFS_GROUP_ERR("Failed to allocate memory for bafs_mem_dma*\n");
        goto out_unlock;
    }


    for (i  = 0; i < group->n_ctrls; i++) {
        ret = bafs_ctrl_dma_map_mem(group->ctrls[i], ctx, params.vaddr, &params.n_dma_addrs, params.dma_addrs + (n_dma_addrs_per_ctrl * i), &dmas[i], i);
        if (ret < 0) {
            goto out_unmap_mems;
        }
        if (i                    == 0)
            n_dma_addrs_per_ctrl = params.n_dma_addrs;
    }



    if (copy_to_user(user_params, &params, sizeof(params))) {
        ret = -EFAULT;
        BAFS_GROUP_ERR("Failed to copy params to user\n");
        goto out_unmap_mems;
    }
    spin_unlock(&group->lock);


    return ret;
out_unmap_mems:
    for (i = i - 1; i >= 0; i--) {
        bafs_ctrl_dma_unmap_mem(dmas[i]);
    }
//out_free_mem:
    kfree(dmas);
out_unlock:
    spin_unlock(&group->lock);
out:
    return ret;
}

static long
bafs_group_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    long ret = 0;

    void __user*      argp = (void __user*) arg;
    struct bafs_group* group;
    struct bafs_ctx* ctx;

    struct bafs_group_ctx* group_ctx = file->private_data;

    if (!group_ctx) {
        ret = -EINVAL;
        goto out;
    }

    group = group_ctx->group;
    ctx = group_ctx->ctx;
    bafs_get_group(group);

    BAFS_GROUP_DEBUG("IOCTL called \t cmd = %u\n", cmd);

    if (_IOC_TYPE(cmd) != BAFS_GROUP_IOCTL) {

        ret = -EINVAL;

        BAFS_GROUP_ERR("Invalid IOCTL commad type = %u\n", _IOC_TYPE(cmd));
        goto out_release_group;
    }

    switch (cmd) {
    case BAFS_GROUP_IOC_REG_MEM:
        ret = bafs_core_reg_mem(argp, ctx);
        if (ret < 0) {
            BAFS_CTRL_ERR("IOCTL to register memory failed\n");
            goto out;
        }
        break;
    case BAFS_GROUP_IOC_DMA_MAP_MEM:
        ret = bafs_group_dma_map_mem(group, ctx, argp);
        if (ret < 0) {
            BAFS_GROUP_ERR("IOCTL to dma map memory failed\n");
            goto out_release_group;
        }
        break;
    default:
        ret = -EINVAL;
        BAFS_GROUP_ERR("Invalid IOCTL cmd \t cmd = %u\n", cmd);
        goto out_release_group;
        break;
    }

    ret = 0;
out_release_group:
    bafs_put_group(group);
out:
    return ret;
}


static int
bafs_group_open(struct inode* inode, struct file* file)
{
    int                ret = 0;

    struct bafs_group_ctx* group_ctx;
    struct bafs_ctx* ctx;
    struct bafs_group* group = container_of(inode->i_cdev, struct bafs_group, cdev);

    if (!group) {
        ret = -EINVAL;
        goto out;
    }
    bafs_get_group(group);

    group_ctx = kzalloc(sizeof(*group_ctx), GFP_KERNEL);
    if(group_ctx == NULL) {
        ret = -ENOMEM;
        goto out_put_group;
    }

    ctx = bafs_get_ctx();
    if (ctx == NULL) {
        ret = -EINVAL;
        goto out_free_group_ctx;
    }

    group_ctx->group = group;
    group_ctx->ctx = ctx;

    file->private_data = group_ctx;
    return ret;
out_free_group_ctx:
    kfree(group_ctx);
out_put_group:
    bafs_put_group(group);
out:
    return ret;
}

static int
bafs_group_release(struct inode* inode, struct file* file)
{
    int ret = 0;
    struct bafs_ctx* ctx;
    struct bafs_mem*      mem;
    struct bafs_mem*      next;
    struct bafs_group_ctx* group_ctx = (struct bafs_group_ctx*) file->private_data;

    if (!group_ctx) {
        ret = -EINVAL;
        goto out;
    }

    ctx = group_ctx->ctx;
    spin_lock(&ctx->lock);
    list_for_each_entry_safe(mem, next, &ctx->mem_list, mem_list) {
        spin_lock(&mem->lock);
        if (mem->state == STALE) {

            list_del_init(&mem->mem_list);

            xa_erase(&ctx->bafs_mem_xa, mem->mem_id);
            BAFS_GROUP_DEBUG("Deleting Stale mem registeration\n");
            spin_unlock(&mem->lock);
            kfree_rcu(mem, rh);
            bafs_put_ctx(ctx);

        }
        else {
            spin_unlock(&mem->lock);
        }


    }

    spin_unlock(&ctx->lock);

    bafs_put_ctx(ctx);
    BAFS_GROUP_DEBUG("Closed core and cleaned ctx\n");

    bafs_put_group(group_ctx->group);
    kfree(group_ctx);
    file->private_data = NULL;
    ret = 0;
    return ret;
out:
    return ret;
}

static int
bafs_group_mmap(struct file* file, struct vm_area_struct* vma)
{
    int ret                        = 0;
    int                          i = 0;

    unsigned long cur_map_size  = 0;
    unsigned long      map_size = 0;
    struct bafs_group* group;
    struct bafs_ctx* ctx;
    struct bafs_group_ctx* group_ctx = (struct bafs_group_ctx*) file->private_data;


    if (!group_ctx) {
        ret = -EINVAL;
        goto out;
    }

    group  = group_ctx->group;
    ctx = group_ctx->ctx;

    if (vma->vm_pgoff == 0) {

        bafs_get_group(group);
        spin_lock(&group->lock);

        for (i = 0; i < group->n_ctrls; i++) {
            ret = bafs_ctrl_mmap(group->ctrls[i], vma, vma->vm_start + map_size, &cur_map_size);
            if (ret < 0) {
                goto out_unlock;
            }
            map_size += cur_map_size;
        }

        spin_unlock(&group->lock);
        bafs_put_group(group);
    }
    else {
        ret = pin_bafs_mem(vma, ctx);
        if (ret < 0) {
            BAFS_GROUP_ERR("Failed to mmap memory \t err = %d\n", ret);
            goto out;
        }
    }
    ret = 0;
    return ret;
out_unlock:
    spin_unlock(&group->lock);
    bafs_put_group(group);
out:
    return ret;
}

const struct file_operations bafs_group_fops = {

    .owner          = THIS_MODULE,
    .open           = bafs_group_open,
    .unlocked_ioctl = bafs_group_ioctl,
    .release        = bafs_group_release,
    .mmap           = bafs_group_mmap,

};


int
bafs_group_alloc(struct bafs_group ** out, int bafs_major, struct device * bafs_core_device,
                 size_t n_ctrls, ctrl_name * ctrls)
{
    struct bafs_group * group = NULL;
    struct device ** ctrl_devices;
    const char * device_class_name;
    int i, j, ret;

    group = kzalloc(sizeof(*group), GFP_KERNEL);
    if(group == NULL) {
        BAFS_CORE_ERR("Failed to allocate memory for bafs_group\n");
        return -ENOMEM;
    }

    ctrl_devices = kzalloc(n_ctrls * sizeof(*ctrl_devices), GFP_KERNEL);
    if(ctrl_devices == NULL) {
        ret = -ENOMEM;
        goto out_free_group;
    }

    get_device(bafs_core_device);
    for (j = 0; j < n_ctrls; j++) {
        ctrl_devices[j] = device_find_child_by_name(bafs_core_device, ctrls[j]);
        if (!ctrl_devices[j]) {
            BAFS_CORE_ERR("Failed to find ctrl device: %s\n", ctrls[j]);
            ret = -EINVAL;
            goto out_put_devices;
        }
        device_class_name = ctrl_devices[j]->class ? ctrl_devices[j]->class->name : "";
        ret = strncmp(device_class_name, BAFS_CTRL_CLASS_NAME, strlen(BAFS_CTRL_CLASS_NAME));
        if (ret != 0) {
            BAFS_CORE_ERR("Failed to find ctrl device: %s\n", ctrls[j]);
            ret = -EINVAL;
            goto out_put_devices;
        }

    }

    spin_lock_init(&group->lock);

    group->ctrls = kzalloc(n_ctrls * sizeof(*(group->ctrls)), GFP_KERNEL);
    if (!group->ctrls) {
        ret = -ENOMEM;
        BAFS_CORE_ERR("Failed to allocate memory for bafs_group\n");
        goto out_put_devices;
    }

    for(i = 0; i < n_ctrls; i++) {
        group->ctrls[i] = (struct bafs_ctrl *) dev_get_drvdata(ctrl_devices[i]);
        if (!group->ctrls[i]) {
            ret = -EFAULT;
            BAFS_CORE_ERR("Failed to find ctrl device: %s\n", ctrls[i]);
            goto out_free_ctrls;
        }
        bafs_get_ctrl(group->ctrls[i]);
    }


    ret = ida_simple_get(&bafs_group_ida, 0, 0, GFP_KERNEL);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to get group instance id \t err = %d\n", ret);
        goto out_free_ctrls;
    }
    group->group_id = ret;
    group->n_ctrls  = n_ctrls;

    cdev_init(&group->cdev, &bafs_group_fops);
    group->cdev.owner = THIS_MODULE;

    group->major = MAJOR(bafs_major);
    ret = bafs_get_minor_number();
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to get minor instance id \t err = %d\n", ret);
        goto out_group_id_put;
    }
    group->minor = ret;

    ret = cdev_add(&group->cdev, MKDEV(group->major, group->minor), 1);
    if (ret < 0) {
        BAFS_CORE_ERR("Failed to init group cdev \t err = %d\n", ret);
        goto out_minor_put;
    }
    group->core_dev = bafs_core_device;

    group->device = device_create(bafs_group_class, bafs_core_device, MKDEV(group->major,
                                  group->minor), group, BAFS_GROUP_DEVICE_NAME, group->group_id);
    if(IS_ERR(group->device)) {
        ret = PTR_ERR(group->device);
        BAFS_CORE_ERR("Failed to create group device %s%d with minor %d \t err = %d\n", BAFS_GROUP_DEVICE_NAME, group->group_id, group->minor, ret);
        goto out_delete_cdev;
    }
    kref_init(&group->ref);
    BAFS_CORE_INFO("Created group device %s\n", dev_name(group->device));


    for (j = 0; j < n_ctrls; j++) {
        put_device(ctrl_devices[j]);
    }

    kfree(ctrl_devices);

    *out = group;
    return 0;

out_delete_cdev:
    cdev_del(&group->cdev);
out_minor_put:
    bafs_put_minor_number(group->minor);
out_group_id_put:
    ida_simple_remove(&bafs_group_ida, group->group_id);
out_free_ctrls:
    for(i = i - 1; i >= 0; i--) {
        WARN_ON(group->ctrls[i] == NULL);
        if(group->ctrls[i] == NULL) continue;

        bafs_ctrl_release(group->ctrls[i]);
    }
    kfree(group->ctrls);
out_put_devices:
    for(j = j - 1; j >= 0; j--) {

        put_device(ctrl_devices[j]);
    }
    put_device(bafs_core_device);
    kfree(ctrl_devices);
out_free_group:
    kfree(group);

    return ret;
}


int
bafs_group_init()
{
    int ret = 0;

    //create group class
    bafs_group_class = class_create(THIS_MODULE, BAFS_GROUP_CLASS_NAME);

    if (IS_ERR(bafs_group_class)) {
        ret = PTR_ERR(bafs_group_class);
        BAFS_CORE_ERR("Failed to create group class \t err = %d\n", ret);
    }

    return ret;
}

void
bafs_group_fini()
{
    class_destroy(bafs_group_class);
}
