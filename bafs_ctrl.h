#ifndef __BAFS_CTRL_H__
#define __BAFS_CTRL_H__

#include "bafs_util.h"
#include "bafs_mem.h"
#include "bafs_ctrl_ioctl.h"
#include "bafs_types.h"



static long bafs_ctrl_dma_map_mem(void __user* user_params) {

    long ret = 0;
    //struct bafs_mem* mem;

/*     struct BAFS_CORE_IOC_REG_MEM_PARAMS params; */

/*     if (copy_from_user(&params, user_params, sizeof(params))) { */
/*         ret = -EFAULT; */
/*         BAFS_CORE_ERR("Failed to copy params from user\n"); */
/*         goto out; */
/*     } */

/*     mem     = kzalloc(sizeof(*mem), GFP_KERNEL); */
/*     if (!mem){ */
/*         ret = -ENOMEM; */
/*         BAFS_CORE_ERR("Failed to allocate memory for bafs_mem\n"); */
/*         goto out; */
/*     } */

/*     ret = xa_alloc(bafs_mem_xa, &(params.handle), mem, xa_limit_32b, GFP_KERNEL); */
/*     if (ret < 0) { */
/*         ret = -ENOMEM; */
/*         BAFS_CORE_ERR("Failed to allocate entry in bafs_mem_xa\n"); */
/*         goto out_delete_mem; */
/*     } */

/*     if (copy_to_user(user_params, &params,, sizeof(params))) { */
/*         ret = -EFAULT; */
/*         BAFS_CORE_ERR("Failed to copy params to user\n"); */
/*         goto out_erase_xa_entry; */
/*     } */

/*     init_bafs_mem(mem, &params); */

/*     return ret; */

/* out_erase_xa_entry: */
/*     xa_erase(bafs_mem_xa, params.handle); */
/* out_delete_mem: */
/*     kfree(mem); */
/* out: */
    return ret;
}



static long bafs_ctrl_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {

    long ret = 0;

    void __user* argp = (void __user*) arg;


    BAFS_CTRL_DEBUG("IOCTL called \t cmd = %u\n", cmd);

    if (_IOC_TYPE(cmd) != BAFS_CTRL_IOCTL) {
        ret             = -EINVAL;
        BAFS_CTRL_ERR("Invalid IOCTL commad type = %u\n", _IOC_TYPE(cmd));
        goto out;
    }

    switch (cmd) {
    case BAFS_CTRL_IOC_DMA_MAP_MEM:
        ret = bafs_ctrl_dma_map_mem(argp);
        if (ret < 0) {
            BAFS_CTRL_ERR("IOCTL to dma map memory failed\n");
            goto out;
        }
        break;
    default:
        ret = -EINVAL;
        BAFS_CTRL_ERR("Invalid IOCTL cmd \t cmd = %u\n", cmd);
        goto out;
        break;
    }

    return ret;
out:
    return ret;
}

static const struct file_operations bafs_ctrl_fops = {

    .owner          = THIS_MODULE,
    .unlocked_ioctl = bafs_ctrl_ioctl,
//    .mmap         = bafs_core_mmap,

};





#endif                          // __BAFS_CTRL_H__
