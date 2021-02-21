#ifndef __BAFS_MEM_H__
#define __BAFS_MEM_H__

#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>

#include <nv-p2p.h>

#include "bafs_core_ioctl.h"
#include "bafs_util.h"
#include "bafs_types.h"

static struct xarray bafs_mem_xa;




int pin_bafs_cpu_mem(struct bafs_mem* mem, struct vm_area_struct* vma) {

    int ret = 0;
    int count;
    int i;
    UNUSED(vma);

    mem->page_size                     = PAGE_SIZE;
    mem->page_shift                    = PAGE_SHIFT;
    mem->n_pages                       = (mem->size + mem->page_size - 1) >> mem->page_shift;
    mem->page_mask                     = ~(mem->page_size - 1);
    if ((mem->vaddr & mem->page_mask) != mem->vaddr) {
        ret                            = -EINVAL;
        goto out;
    }
    mem->cpu_page_table = (struct page**) kcalloc(mem->n_pages, sizeof(struct page*), GFP_KERNEL);
    if (!mem->cpu_page_table){
        ret             = -ENOMEM;
        BAFS_CORE_DEBUG("Failed to pin cpu memory due to lack of memory\n");
        goto out;
    }


    ret = get_user_pages(mem->vaddr, mem->n_pages, FOLL_WRITE, mem->cpu_page_table, NULL);


    if (ret <= 0) {
        ret = -ENOMEM;
        BAFS_CORE_DEBUG("Failed to pin cpu memory due to get_user_pages failure\n");
        goto out_delete_page_table;
    }

    if ((ret > 0) && (ret < mem->n_pages)) {
        count = ret;
        BAFS_CORE_DEBUG("Failed to pin cpu memory due get_user_pages only getting %i pages when %lu were requested\n", count, mem->n_pages);
        ret   = -ENOMEM;
        goto out_clean_page_table;
    }

    ret = 0;
    return ret;

out_clean_page_table:
    for (i = 0; i < count; i++)
        put_page(mem->cpu_page_table[i]);
out_delete_page_table:
    kfree(mem->cpu_page_table);
    mem->cpu_page_table = NULL;
out:
    return ret;
}


static void __bafs_mem_release(struct kref* ref) {
    struct bafs_mem* mem;

    mem = container_of(ref, struct bafs_mem, ref_count);
    xa_erase(&bafs_mem_xa, mem->mem_id);

    kfree_rcu(mem, rcu_head);
}

static void release_bafs_mem_cuda(void* data) {
    struct bafs_mem* mem;

    struct bafs_mem_dma* dma;
    struct bafs_mem_dma* next;

    mem = (struct bafs_mem*) data;


    spin_lock(&mem->lock);
    list_for_each_entry_safe(dma, next, &mem->dma_list, dma_list) {
        nvidia_p2p_free_dma_mapping(dma->cuda_mapping);

        kfree_rcu(dma, rcu_head);
        kref_put(&mem->ref_count, __bafs_mem_release);
    }
    nvidia_p2p_free_page_table(mem->cuda_page_table);
    spin_unlock(&mem->lock);


    kref_put(&mem->ref_count, __bafs_mem_release);


}

int pin_bafs_cuda_mem(struct bafs_mem* mem, struct vm_area_struct* vma) {

    int      ret = 0;
    unsigned i;
    unsigned map_size;

    ret = nvidia_p2p_get_pages(0, 0, mem->vaddr, mem->size, &mem->cuda_page_table,
                               release_bafs_mem_cuda, mem);
    if(ret < 0) {
        goto out;
    }


    if(!NVIDIA_P2P_PAGE_TABLE_VERSION_COMPATIBLE(mem->cuda_page_table)){
        ret = -EFAULT;
        BAFS_CORE_DEBUG("Failed to pin cuda memory due to incompatible page table version\n");
        goto out_delete_page_table;
    }

    switch (mem->cuda_page_table->page_size) {
    case NVIDIA_P2P_PAGE_SIZE_4KB:
        mem->page_size  = 4*1024;
        mem->page_shift = 12;
        break;
    case NVIDIA_P2P_PAGE_SIZE_64KB:
        mem->page_size  = 64*1024;
        mem->page_shift = 16;
        break;
    case NVIDIA_P2P_PAGE_SIZE_128KB:
        mem->page_size  = 128*1024;
        mem->page_shift = 17;
        break;
    default:
        ret             = -EINVAL;
        BAFS_CORE_DEBUG("Failed to pin cuda memory due to invalid page size\n");
        goto out_delete_page_table;

    }

    mem->page_mask = ~(mem->page_size - 1);


    if ((mem->vaddr & mem->page_mask) != mem->vaddr) {
        ret                            = -EINVAL;
        BAFS_CORE_DEBUG("Failed to pin cuda memory due to unaligned vaddr\n");
        goto out_delete_page_table;
    }

     mem->n_pages = (mem->size + mem->page_size - 1) >> mem->page_size;

    if (mem->n_pages != mem->cuda_page_table->entries) {
        ret = -ENOMEM;
        BAFS_CORE_DEBUG("Failed to pin cuda memory due to unavailable pages\n");
        goto out_delete_page_table;
    }

    vma->vm_page_prot  = pgprot_noncached(vma->vm_page_prot);
    for (i             = 0; i < mem->n_pages; i++) {
        map_size       = mem->page_size;
        if ((i*mem->page_size) > mem->size) {
            map_size  -= ((i*mem->page_size) - mem->size);
        }
        ret = io_remap_pfn_range(vma, mem->vaddr + (i*mem->page_size),
                                 __phys_to_pfn(mem->cuda_page_table->pages[i]->physical_address),
                                 map_size, vma->vm_page_prot);
        if (!ret) {
            BAFS_CORE_DEBUG("Failed to pin cuda memory due to failure to map cuda memory to process address space\n");
            goto out_delete_page_table;
        }
    }

    ret = 0;
    return ret;

out_delete_page_table:
    nvidia_p2p_put_pages(0, 0, mem->vaddr, mem->cuda_page_table);
    mem->cuda_page_table = NULL;
out:
    return ret;
}

int pin_bafs_mem(struct vm_area_struct* vma) {

    int ret = 0;

    struct bafs_mem* mem;
    bafs_mem_hnd_t   mem_id;

    mem_id = vma->vm_pgoff;
    mem    = (struct bafs_mem*) xa_load(&bafs_mem_xa, mem_id);

    mem->vaddr = vma->vm_start;

    vma->vm_flags |= VM_DONTCOPY;
    vma->vm_flags |= VM_DONTEXPAND;

    switch (mem->loc) {

    case CPU:
        ret = pin_bafs_cpu_mem(mem, vma);
        if (!ret)
            goto out;
        break;

    case CUDA:
        ret = pin_bafs_cuda_mem(mem, vma);
        if (!ret)
            goto out;
        break;

    default:
        ret = -EINVAL;
        goto out;
        break;
    }


out:
    return ret;
}

void init_bafs_mem(struct bafs_mem* mem, struct BAFS_CORE_IOC_REG_MEM_PARAMS* params) {
    spin_lock_init(&mem->lock);
    INIT_LIST_HEAD(&mem->dma_list);
    kref_init(&mem->ref_count);

    mem->size   = params->size;
    mem->loc    = params->loc;
    mem->mem_id = params->handle;
}




static long bafs_core_reg_mem(void __user* user_params) {

    long                                ret = 0;
    struct bafs_mem*                    mem;
    struct BAFS_CORE_IOC_REG_MEM_PARAMS params;

    if (copy_from_user(&params, user_params, sizeof(params))) {
        ret = -EFAULT;
        BAFS_CORE_ERR("Failed to copy params from user\n");
        goto out;
    }

    mem     = kzalloc(sizeof(*mem), GFP_KERNEL);
    if (!mem){
        ret = -ENOMEM;
        BAFS_CORE_ERR("Failed to allocate memory for bafs_mem\n");
        goto out;
    }



    ret     = xa_alloc(&bafs_mem_xa, &(params.handle), mem, xa_limit_32b, GFP_KERNEL);
    if (ret < 0) {
        ret = -ENOMEM;
        BAFS_CORE_ERR("Failed to allocate entry in bafs_mem_xa\n");
        goto out_delete_mem;
    }

    if (copy_to_user(user_params, &params, sizeof(params))) {
        ret = -EFAULT;
        BAFS_CORE_ERR("Failed to copy params to user\n");
        goto out_erase_xa_entry;
    }

    init_bafs_mem(mem, &params);

    return ret;

out_erase_xa_entry:
    xa_erase(&bafs_mem_xa, params.handle);
out_delete_mem:
    kfree(mem);
out:
    return ret;
}



static void unmap_dma(struct bafs_mem_dma* dma) {
    unsigned         i;
    unsigned map_size;
    struct bafs_mem* mem;
    struct pci_dev*  pdev;

    mem = dma->mem;

    spin_lock(&dma->lock);
    list_del(&dma->dma_list);

    switch (mem->loc) {
    case CPU:
        for (i = 0; i < mem->n_pages; i++) {
            map_size       = dma->map_size;
            if ((i*dma->map_size) > mem->size) {
                map_size  -= ((i*dma->map_size) - mem->size);
            }
            dma_unmap_single(dma->ctrl->dev, dma->addrs[i], map_size, DMA_BIDIRECTIONAL);
        }

        kfree(dma->addrs);


        break;
    case CUDA:
        nvidia_p2p_dma_unmap_pages(dma->ctrl->pdev, mem->cuda_page_table, dma->cuda_mapping);
        break;
    default:
        break;

    }

    pdev = dma->ctrl->pdev;
    bafs_put_ctrl(dma->ctrl);
    put_device(&pdev->dev);
    spin_unlock(&dma->lock);
}

static void bafs_mem_release(struct vm_area_struct* vma) {

    unsigned             i;
    bafs_mem_hnd_t       mem_id;
    struct bafs_mem*     mem;
    struct bafs_mem_dma* dma;
    struct bafs_mem_dma* next;


    //mem  = (struct bafs_mem*) vma->vm_private_data;
    mem_id = vma->vm_pgoff;
    mem    = (struct bafs_mem*) xa_load(&bafs_mem_xa, mem_id);

    if (mem == NULL) {
        goto out;
    }

    spin_lock(&mem->lock);
    list_for_each_entry_safe(dma, next, &mem->dma_list, dma_list) {
        unmap_dma(dma);
        kfree_rcu(dma, rcu_head);
        kref_put(&mem->ref_count, __bafs_mem_release);
    }
    switch (mem->loc) {
    case CPU:
        for (i = 0; i < mem->n_pages; i++)
            put_page(mem->cpu_page_table[i]);
        kfree(mem->cpu_page_table);
        break;
    case CUDA:
        nvidia_p2p_put_pages(0, 0, mem->vaddr, mem->cuda_page_table);
        break;
    default:
        break;

    }

    spin_unlock(&mem->lock);


    kref_put(&mem->ref_count, __bafs_mem_release);

out:
    return;
}


static const struct vm_operations_struct bafs_mem_fops = {

    .close = bafs_mem_release,

};




#endif                          // __BAFS_MEM_H__
