#ifndef _BAFS_H_
#define _BAFS_H_

#include <linux/bafs.h>


#ifdef __cplusplus
extern "C" {
#endif


struct bafs_dma_t {
    void* vaddr;
    void** dma_addrs;
    unsigned n_dma_addrs;
};

struct bafs_ctrl_t {
    int fd;
    void* ctrl_regs;
    const char* ctrl_dev_name;
    int type;
};

/* BAFS CORE */
int bafs_core_create_group(unsigned n_ctrls, char* ctrl_names[], char* ret_group_name);

int bafs_core_delete_group(char* group_name);



int bafs_ctrl_open(const char* ctrl_dev_name, struct bafs_ctrl_t* ctrl_handle);


int bafs_ctrl_reg_mem(unsigned size, unsigned loc, struct bafs_ctrl_t* ctrl_handle, bafs_mem_hnd_t* ret_handle);
int bafs_ctrl_pin_mem(void** addr, unsigned size, struct bafs_ctrl_t* ctrl_handle, bafs_mem_hnd_t handle);

int bafs_ctrl_map(void** addr, unsigned size, unsigned loc, struct bafs_ctrl_t* ctrl_handle);

int bafs_ctrl_dma_map_mem(void* vaddr, struct bafs_dma_t* dma_handle, struct bafs_ctrl_t* ctrl_handle);



#ifdef __cplusplus
}
#endif

#endif // _BAFS_H_
