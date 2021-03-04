#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <bafs.h>
#include <linux/bafs.h>

#define BAFS_CORE_DEVICE_NAME "bafs"
#define GROUP 1
#define NOT_GROUP 0


extern int errno;

/* BAFS CORE */
static int bafs_core_fd = -1;

static int bafs_core_init(void) {
    bafs_core_fd = open("/dev/" BAFS_CORE_DEVICE_NAME, O_RDWR);
    return bafs_core_fd;
}




int bafs_core_create_group(unsigned int n_ctrls, char* ctrl_names[], char* ret_group_name) {
    int ret = 0;
    unsigned int i = 0;
    struct BAFS_CORE_IOC_CREATE_GROUP_PARAMS params;
    ctrl_name* names;

    if (bafs_core_fd < 0) {
        ret = bafs_core_init();
        if (ret < 0)
            return ret;
    }

    names = (ctrl_name*) calloc(n_ctrls, sizeof(ctrl_name));
    if (names == NULL) {
        ret = ENOMEM;
        return ret;
    }

    for (i = 0; i < n_ctrls; i++) {
        ret = sscanf(ctrl_names[i], "/dev/%s", names[i]);
        if ((ret == EOF) || (ret != 1)) {
            ret = EINVAL;
            free(names);
            return ret;
        }
    }

    params.n_ctrls = n_ctrls;
    params.ctrls = names;
    params.group_name = ret_group_name;

    ret = ioctl(bafs_core_fd, BAFS_CORE_IOC_CREATE_GROUP, &params);
    if (ret) {
        free(names);
        ret = errno;
        return ret;
    }

    free(names);
    return 0;

}

int bafs_core_delete_group(char* group_name) {
    int ret = 0;
    struct BAFS_CORE_IOC_DELETE_GROUP_PARAMS params;
    ctrl_name group_dev_name;

    if (bafs_core_fd < 0) {
        ret = bafs_core_init();
        if (ret < 0)
            return ret;
    }

    ret = sscanf(group_name, "/dev/%s", group_dev_name);
    if ((ret == EOF) || (ret != 1)) {
        ret = EINVAL;
        return ret;
    }

    params.group_name = group_dev_name;

    ret = ioctl(bafs_core_fd, BAFS_CORE_IOC_DELETE_GROUP, &params);
    if (ret) {

        ret = errno;
        return ret;
    }

    return 0;

}


/* BAFS CTRL/GROUP */

int bafs_ctrl_open(const char* ctrl_dev_name, struct bafs_ctrl_t* ctrl_handle) {
    int ret = 0;
    int fd;
    char c_or_g;
    int dev_id;

    ret = sscanf(ctrl_dev_name, "/dev/bafs%c%d", &c_or_g, &dev_id);
    if ((ret == EOF) || (ret != 2)) {
        ret = EINVAL;
        return ret;
    }
    if (c_or_g == 'g') {
        ctrl_handle->type = GROUP;
    }
    else if (c_or_g == 'c') {
        ctrl_handle->type = NOT_GROUP;
    }
    else {
        ret = EINVAL;
        return ret;
    }
    fd =  open(ctrl_dev_name, O_RDWR);
    if (fd < 0) {
        ret = errno;
        return ret;
    }

    ctrl_handle->ctrl_dev_name = ctrl_dev_name;
    ctrl_handle->fd = fd;

    return 0;
}


int bafs_ctrl_reg_mem(unsigned size, unsigned loc, struct bafs_ctrl_t* ctrl_handle, bafs_mem_hnd_t* ret_handle) {
    int ret = 0;
    struct BAFS_IOC_REG_MEM_PARAMS params;

    if (ctrl_handle->fd < 0) {
        ret = EINVAL;
        fprintf(stderr, "ctrl fd invalid: %d\n", ctrl_handle->fd);
        return ret;
    }

    params.size = size;
    params.loc = loc;
    params.handle = 0;

    if (ctrl_handle->type == GROUP) {

        ret = ioctl(ctrl_handle->fd, BAFS_GROUP_IOC_REG_MEM, &params);
        if (ret) {
            ret = errno;
            return ret;
        }


    }
    else if (ctrl_handle->type == NOT_GROUP) {

        ret = ioctl(ctrl_handle->fd, BAFS_CTRL_IOC_REG_MEM, &params);
        if (ret) {
            ret = errno;
            return ret;
        }


    }
    else {
        ret = EINVAL;
        return ret;
    }

    *ret_handle = params.handle;

    return 0;

}

int bafs_ctrl_pin_mem(void** addr, unsigned size, struct bafs_ctrl_t* ctrl_handle, bafs_mem_hnd_t handle) {
    int ret = 0;
    void* addr_;

    if (ctrl_handle->fd < 0) {
        ret = EINVAL;
        fprintf(stderr, "ctrl fd invalid: %d\n", ctrl_handle->fd);
        return ret;
    }

    addr_ = mmap(*addr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, ctrl_handle->fd, handle);
    if (addr_ == MAP_FAILED) {
        ret = errno;
        fprintf(stderr, "mmap failed: %d\n", ret);
        return ret;
    }
    *addr = addr_;

    return 0;
}

int bafs_ctrl_map(void** addr, unsigned size, unsigned loc, struct bafs_ctrl_t* ctrl_handle) {
    int ret = 0;
    bafs_mem_hnd_t handle;

    if (ctrl_handle->fd < 0) {
        ret = EINVAL;
        fprintf(stderr, "ctrl fd invalid: %d\n", ctrl_handle->fd);
        return ret;
    }
    ret = bafs_ctrl_reg_mem(size, loc, ctrl_handle, &handle);
    if (ret) {
        return ret;
    }

    ret = bafs_ctrl_pin_mem(addr, size, ctrl_handle, handle);
    if (ret) {
        return ret;
    }

    return 0;
}




int bafs_ctrl_dma_map_mem(void* vaddr, struct bafs_dma_t* dma_handle, struct bafs_ctrl_t* ctrl_handle) {
    int ret = 0;

    struct BAFS_IOC_DMA_MAP_MEM_PARAMS params;


    if (ctrl_handle->fd < 0) {
        ret = EBADF;
        return ret;
    }

    params.vaddr = (unsigned long) vaddr;
    params.dma_addrs = (unsigned long*) dma_handle->dma_addrs;
    params.n_dma_addrs = dma_handle->n_dma_addrs;

    if (ctrl_handle->type == GROUP) {

        ret = ioctl(ctrl_handle->fd, BAFS_GROUP_IOC_DMA_MAP_MEM, &params);
        if (ret) {
            ret = errno;
            return ret;
        }


    }
    else if (ctrl_handle->type == NOT_GROUP) {

        ret = ioctl(ctrl_handle->fd, BAFS_CTRL_IOC_DMA_MAP_MEM, &params);
        if (ret) {
            ret = errno;
            return ret;
        }


    }
    else {
        ret = EINVAL;
        return ret;
    }

    dma_handle->vaddr = vaddr;
    dma_handle->n_dma_addrs = params.n_dma_addrs;

    return 0;
}
