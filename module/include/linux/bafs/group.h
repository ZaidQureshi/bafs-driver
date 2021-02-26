#ifndef _LINUX_BAFS_GROUP_H_
#define _LINUX_BAFS_GROUP_H_

#include <linux/cdev.h>
#include <asm/uaccess.h>

#include <linux/bafs.h>

#include "types.h"
#include "util.h"
#include "release.h"

long bafs_group_dma_map_mem(struct bafs_group* group, void __user* user_params) {

    long     ret                  = 0;
    int      i                    = 0;
    uint64_t n_dma_addrs_per_ctrl = 0;

    struct bafs_mem_dma**                    dmas;

    struct BAFS_GROUP_IOC_DMA_MAP_MEM_PARAMS params = {0};


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
        ret = bafs_ctrl_dma_map_mem(group->ctrls[i], params.vaddr, &params.n_dma_addrs, params.dma_addrs + (n_dma_addrs_per_ctrl * i), &dmas[i], i);
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

long bafs_group_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {

    long ret = 0;

    void __user*       argp  = (void __user*) arg;
    struct bafs_group* group = file->private_data;

    if (!group) {
        ret = -EINVAL;
        goto out;
    }
    bafs_get_group(group);

    BAFS_GROUP_DEBUG("IOCTL called \t cmd = %u\n", cmd);

    if (_IOC_TYPE(cmd) != BAFS_GROUP_IOCTL) {

        ret = -EINVAL;

        BAFS_GROUP_ERR("Invalid IOCTL commad type = %u\n", _IOC_TYPE(cmd));
        goto out_release_group;
    }

    switch (cmd) {
    case BAFS_GROUP_IOC_DMA_MAP_MEM:
        ret = bafs_group_dma_map_mem(group, argp);
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
    bafs_put_group(group, __bafs_group_release);
out:
    return ret;
}


int bafs_group_open(struct inode* inode, struct file* file) {

    int                ret = 0;

    struct bafs_group* group = container_of(inode->i_cdev, struct bafs_group, cdev);

    if (!group) {
        ret = -EINVAL;
        goto out;
    }
    bafs_get_group(group);
    file->private_data = group;
    return ret;
out:
    return ret;
}

int bafs_group_release(struct inode* inode, struct file* file) {

    int ret = 0;

    struct bafs_group* group = (struct bafs_group*) file->private_data;

    if (!group) {
        ret = -EINVAL;
        goto out;
    }
    bafs_put_group(group, __bafs_group_release);
    return ret;
out:
    return ret;
}

int bafs_group_mmap(struct file* file, struct vm_area_struct* vma) {
    int ret                        = 0;
    int                          i = 0;

    unsigned long cur_map_size  = 0;
    unsigned long      map_size = 0;
    struct bafs_group* group   = (struct bafs_group*) file->private_data;

    if (!group) {
        ret = -EINVAL;
        goto out;
    }
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
    bafs_put_group(group, __bafs_group_release);
    ret = 0;
    return ret;
out_unlock:
    spin_unlock(&group->lock);
    bafs_put_group(group, __bafs_group_release);
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





#endif                          // __BAFS_GROUP_H__
