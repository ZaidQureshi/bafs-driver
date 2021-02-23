#ifndef __BAFS_CTRL_H__
#define __BAFS_CTRL_H__

#include "bafs_util.h"
#include "bafs_mem.h"
#include "bafs_ctrl_ioctl.h"
#include "bafs_types.h"
#include "bafs_release.h"




static int __bafs_ctrl_dma_map_mem(struct bafs_ctrl* ctrl, struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS* params, struct bafs_mem_dma** dma_) {
    int ret = 0;

    struct bafs_mem*     mem;
    struct vm_area_struct* vma;
    struct bafs_mem_dma* dma;
    unsigned             map_gran;
    int                  i = 0;

    vma = find_vma(current->mm, params->vaddr);
    if (!vma) {
        ret = -EINVAL;
        goto out;
    }
    mem     = (struct bafs_mem*) vma->vm_private_data;
    if (!mem) {
        ret = -EINVAL;
        goto out;
    }


    *dma_   = kzalloc(sizeof(*dma), GFP_KERNEL);
    if (!(*dma_)){
        ret = -ENOMEM;
        BAFS_CTRL_ERR("Failed to allocate memory for bafs_mem_dma\n");
        goto out;
    }

    dma = *dma_;

    kref_get(&mem->ref);

    dma->dev = bafs_get_ctrl(ctrl);

    INIT_LIST_HEAD(&dma->dma_list);


    dma->ctrl = ctrl;

    spin_lock(&mem->lock);
    list_add(&dma->dma_list, &mem->dma_list);
    spin_unlock(&mem->lock);




    switch (mem->loc) {
    case CPU:
        dma->map_gran = mem->page_size;
        dma->addrs    = (dma_addr_t*) kcalloc(mem->n_pages, sizeof(dma_addr_t*), GFP_KERNEL);
        if (!dma->addrs) {
            ret       = -ENOMEM;
            goto out_delete_mem;
        }
        for (i = 0; i < mem->n_pages; i++) {
            map_gran      = dma->map_gran;
            if ((i*dma->map_gran) > mem->size) {
                map_gran -= ((i*dma->map_gran) - mem->size);
            }
            dma->addrs[i] = dma_map_single(dma->ctrl->dev, page_to_virt(mem->cpu_page_table[i]), map_gran, DMA_BIDIRECTIONAL);
            if (dma_mapping_error(dma->ctrl->dev, dma->addrs[i])) {
                ret       = -EFAULT;
                goto out_unmap;
            }
        }
        params->n_dma_addrs = mem->n_pages;

        if (copy_to_user(params->dma_addrs, dma->addrs, params->n_dma_addrs*sizeof(dma_addr_t))) {
            ret = -EFAULT;
            BAFS_CTRL_ERR("Failed to copy dma addrs to user\n");
            goto out_unmap;
        }

        break;
    case CUDA:
        ret = nvidia_p2p_dma_map_pages(ctrl->pdev, mem->cuda_page_table, &dma->cuda_mapping);
        if (ret != 0) {
            goto out_delete_mem;
        }
        params->n_dma_addrs = dma->cuda_mapping->entries;

        if (copy_to_user(params->dma_addrs, dma->cuda_mapping->dma_addresses, params->n_dma_addrs*sizeof(uint64_t))) {
            ret = -EFAULT;
            BAFS_CTRL_ERR("Failed to copy dma addrs to user\n");
            goto out_unmap;
        }
        break;
    default:
        ret = -EINVAL;
        goto out_delete_mem;
        break;

    }



    return ret;

out_unmap:
    if (mem->loc == CPU) {
        for (i = i-1; i >= 0; i--)
            dma_unmap_single(dma->ctrl->dev, dma->addrs[i], dma->map_gran, DMA_BIDIRECTIONAL);
    }
    else if(mem->loc == CUDA) {
        nvidia_p2p_dma_unmap_pages(dma->ctrl->pdev, mem->cuda_page_table, dma->cuda_mapping);
    }
out_delete_mem:
    spin_lock(&mem->lock);
    list_del_init(&dma->dma_list);
    spin_unlock(&mem->lock);


    kfree(dma);

    bafs_put_ctrl(ctrl, __bafs_ctrl_release);

    kref_put(&mem->ref, __bafs_mem_release);


out:
    return ret;

} 

static void __bafs_ctrl_dma_unmap_mem(struct bafs_mem_dma* dma) {
    struct bafs_mem* mem = dma->mem;
    spin_lock(&mem->lock);
    unmap_dma(dma);
    spin_unlock(&mem->lock);
    kref_put(&mem->ref, __bafs_mem_release);

}

static long bafs_ctrl_dma_map_mem(struct bafs_ctrl* ctrl, void __user* user_params) {

    long ret = 0;

    struct bafs_mem_dma*                    dma;
    struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS params;

    if (copy_from_user(&params, user_params, sizeof(params))) {
        ret = -EFAULT;
        BAFS_CTRL_ERR("Failed to copy params from user\n");
        goto out;
    }

    ret = __bafs_ctrl_dma_map_mem(ctrl, &params, &dma);
    if (ret < 0) {
        goto out;
    }



    if (copy_to_user(user_params, &params, sizeof(params))) {
        ret = -EFAULT;
        BAFS_CTRL_ERR("Failed to copy params to user\n");
        goto out_unmap_memory;
    }


    return ret;
out_unmap_memory:
    __bafs_ctrl_dma_unmap_mem(dma);
out:
    return ret;
}



static long bafs_ctrl_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {

    long ret = 0;

    void __user*      argp = (void __user*) arg;
    struct bafs_ctrl* ctrl = file->private_data;

    if (!ctrl) {
        ret = -EINVAL;
        goto out;
    }
    bafs_get_ctrl(ctrl);

    BAFS_CTRL_DEBUG("IOCTL called \t cmd = %u\n", cmd);

    if (_IOC_TYPE(cmd) != BAFS_CTRL_IOCTL) {

        ret = -EINVAL;

        BAFS_CTRL_ERR("Invalid IOCTL commad type  = %u\n", _IOC_TYPE(cmd));
        goto out_release_ctrl;
    }

    switch (cmd) {
    case BAFS_CTRL_IOC_DMA_MAP_MEM:
        ret = bafs_ctrl_dma_map_mem(ctrl, argp);
        if (ret < 0) {
            BAFS_CTRL_ERR("IOCTL to dma map memory failed\n");
            goto out_release_ctrl;
        }
        break;
    default:
        ret = -EINVAL;
        BAFS_CTRL_ERR("Invalid IOCTL cmd \t cmd = %u\n", cmd);
        goto out_release_ctrl;
        break;
    }

    ret = 0;
out_release_ctrl:
    bafs_put_ctrl(ctrl, __bafs_ctrl_release);
out:
    return ret;
}


static int bafs_ctrl_open(struct inode* inode, struct file* file) {

    int               ret = 0;
    struct bafs_ctrl* ctrl = container_of(inode->i_cdev, struct bafs_ctrl, cdev);

    if (!ctrl) {
        ret = -EINVAL;
        goto out;
    }
    bafs_get_ctrl(ctrl);
    file->private_data = ctrl;
    return ret;
out:
    return ret;
}

static int bafs_ctrl_release(struct inode* inode, struct file* file) {

    int ret = 0;

    struct bafs_ctrl* ctrl = (struct bafs_ctrl*) file->private_data;

    if (!ctrl) {
        ret = -EINVAL;
        goto out;
    }
    bafs_put_ctrl(ctrl, __bafs_ctrl_release);
    return ret;
out:
    return ret;
}

static const struct file_operations bafs_ctrl_fops = {

    .owner          = THIS_MODULE,
    .open           = bafs_ctrl_open,
    .unlocked_ioctl = bafs_ctrl_ioctl,
    .release        = bafs_ctrl_release,
//    .mmap         = bafs_core_mmap,

};





#endif                          // __BAFS_CTRL_H__
