#include <linux/cdev.h>

#include <linux/bafs.h>

#include <linux/bafs/util.h>
#include <linux/bafs/types.h>

static DEFINE_IDA(bafs_minor_ida);
static DEFINE_IDA(bafs_ctrl_ida);

static struct class *   bafs_ctrl_class = NULL;

int
bafs_ctrl_init()
{
    int ret = 0;
    bafs_ctrl_class = class_create(THIS_MODULE, BAFS_CTRL_CLASS_NAME);
    if (IS_ERR(bafs_ctrl_class)) {
        ret = PTR_ERR(bafs_ctrl_class);
        BAFS_CORE_ERR("Failed to create ctrl class \t err = %d\n", ret);
    }
    return ret;
}


void
bafs_ctrl_fini()
{
    if(bafs_ctrl_class == NULL) return;
    class_destroy(bafs_ctrl_class);
    bafs_ctrl_class = NULL;
}

int
bafs_get_minor_number()
{
    return ida_simple_get(&bafs_minor_ida, 0, 0, GFP_KERNEL);
}

void
bafs_put_minor_number(int id)
{
    ida_simple_remove(&bafs_minor_ida, id);
}


static 
void __bafs_ctrl_release(struct kref * ref)
{
    struct bafs_ctrl* ctrl;

    WARN_ON(ref == NULL);
    if(ref == NULL) return;

    ctrl = container_of(ref, struct bafs_ctrl, ref);
    BAFS_CTRL_DEBUG("Removing PCI \t ctrl: %p\n", ctrl);

    BAFS_CORE_DEBUG("Attempting to remove ctrl with id %d major %d minor %d\n", ctrl->ctrl_id, ctrl->major, ctrl->minor);

    device_destroy(bafs_ctrl_class, MKDEV(ctrl->major, ctrl->minor));
    put_device(ctrl->core_dev);
    cdev_del(&ctrl->cdev);



    pci_disable_device(ctrl->pdev);
    pci_release_region(ctrl->pdev, 0);
    pci_clear_master(ctrl->pdev);
    put_device(&ctrl->pdev->dev);
    ida_simple_remove(&bafs_ctrl_ida, ctrl->ctrl_id);
    bafs_put_minor_number(ctrl->minor);

    BAFS_CTRL_DEBUG("Removed PCI \t ctrl: %p\n", ctrl);

    kfree_rcu(ctrl, rh);
}


void
bafs_ctrl_release(struct bafs_ctrl * ctrl)
{
    BAFS_CTRL_DEBUG("In bafs_ctrl_release: %u \t kref_bef: %u\n", ctrl->ctrl_id, kref_read(&ctrl->ref));
    kref_put(&ctrl->ref, __bafs_ctrl_release);
    BAFS_CTRL_DEBUG("In bafs_ctrl_release: %u \t kref_aft: %u\n", ctrl->ctrl_id, kref_read(&ctrl->ref));
}


int
bafs_ctrl_dma_map_mem(struct bafs_ctrl * ctrl, unsigned long vaddr, __u32 * n_dma_addrs,
                      unsigned long __user * dma_addrs_user, struct bafs_mem_dma ** dma_,
                      const int ctrl_id)
{
    int ret = 0;
    int i   = 0;

    struct bafs_mem*       mem;
    struct bafs_mem_dma*   dma;
    unsigned               map_gran;


    mem     = bafs_get_mem(vaddr);
    if (!mem) {
        ret = -EINVAL;
        BAFS_CTRL_ERR("Failed to find bafs_mem obj for dma map\n");
        goto out;
    }


    *dma_   = kzalloc(sizeof(*dma), GFP_KERNEL);
    if (!(*dma_)){
        ret = -ENOMEM;
        BAFS_CTRL_ERR("Failed to allocate memory for bafs_mem_dma\n");
        goto out_put_mem;
    }

    dma = *dma_;


    bafs_get_ctrl(ctrl);

    INIT_LIST_HEAD(&dma->dma_list);


    dma->ctrl = ctrl;
    dma->mem = mem;





    switch (mem->loc) {
    case BAFS_MEM_CPU:
        dma->map_gran = mem->page_size;
        dma->addrs    = (unsigned long *) kcalloc(mem->n_pages, sizeof(unsigned long *), GFP_KERNEL);
        if (!dma->addrs) {
            ret       = -ENOMEM;
            goto out_delete_dma;
        }
        for (i = 0; i < mem->n_pages; i++) {
            map_gran      = dma->map_gran;
            if ((i*dma->map_gran) > mem->size) {
                map_gran -= ((i*dma->map_gran) - mem->size);
            }
            dma->addrs[i] = dma_map_single(&dma->ctrl->pdev->dev, page_to_virt(mem->cpu_page_table[i]), map_gran, DMA_BIDIRECTIONAL);
            if (dma_mapping_error(&dma->ctrl->pdev->dev, dma->addrs[i])) {
                ret       = -EFAULT;
                goto out_unmap;
            }
        }
        if (ctrl_id      == 0)
            *n_dma_addrs  = mem->n_pages;
        else
            *n_dma_addrs += mem->n_pages;


        if (copy_to_user(dma_addrs_user, dma->addrs, (*n_dma_addrs)*sizeof(unsigned long))) {
            ret = -EFAULT;
            BAFS_CTRL_ERR("Failed to copy %u dma addrs to user\n", *n_dma_addrs);
            goto out_unmap;
        }

        break;
    case BAFS_MEM_CUDA:
        ret      = nvidia_p2p_dma_map_pages(ctrl->pdev, mem->cuda_page_table, &dma->cuda_mapping);
        if (ret != 0) {
            BAFS_CTRL_ERR("nvidia_p2p_dma_map_pages failed \t ret = %d\n", ret);
            goto out_delete_dma;
        }
        if (ctrl_id      == 0)
            *n_dma_addrs  = dma->cuda_mapping->entries;
        else
            *n_dma_addrs += dma->cuda_mapping->entries;


        /* if (copy_to_user(dma_addrs_user, dma->cuda_mapping->dma_addresses, (*n_dma_addrs)*sizeof(unsigned long))) { */
        /*     ret = -EFAULT; */
        /*     BAFS_CTRL_ERR("Failed to copy dma addrs to user\n"); */
        /*     goto out_unmap; */
        /* } */
        break;
    default:
        ret = -EINVAL;
        goto out_delete_dma;
        break;

    }
    spin_lock(&mem->lock);
    list_add(&dma->dma_list, &mem->dma_list);
    spin_unlock(&mem->lock);

    ret = 0;
    return ret;

out_unmap:
    if (mem->loc == BAFS_MEM_CPU) {
        for (i    = i - 1; i >= 0; i--)
            dma_unmap_single(&dma->ctrl->pdev->dev, dma->addrs[i], dma->map_gran, DMA_BIDIRECTIONAL);
    }
    else if(mem->loc == BAFS_MEM_CUDA) {
        nvidia_p2p_dma_unmap_pages(dma->ctrl->pdev, mem->cuda_page_table, dma->cuda_mapping);
    }
out_delete_dma:


    kfree(dma);

    bafs_ctrl_release(ctrl);
out_put_mem:
    bafs_mem_put(mem);


out:
    return ret;

} 

void
bafs_ctrl_dma_unmap_mem(struct bafs_mem_dma* dma)
{
    struct bafs_mem* mem = dma->mem;
    spin_lock(&mem->lock);
    unmap_dma(dma);
    spin_unlock(&mem->lock);
    bafs_mem_put(mem);
}

static long
__bafs_ctrl_dma_map_mem(struct bafs_ctrl* ctrl, void __user * user_params)
{
    long ret = 0;

    struct bafs_mem_dma*                    dma;
    struct BAFS_CTRL_IOC_DMA_MAP_MEM_PARAMS params;

    if (copy_from_user(&params, user_params, sizeof(params))) {
        ret = -EFAULT;
        BAFS_CTRL_ERR("Failed to copy params from user\n");
        goto out;
    }

    ret = bafs_ctrl_dma_map_mem(ctrl, params.vaddr, &params.n_dma_addrs, params.dma_addrs, &dma, 0);
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
    bafs_ctrl_dma_unmap_mem(dma);
out:
    return ret;
}



static long
bafs_ctrl_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{

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
        ret = __bafs_ctrl_dma_map_mem(ctrl, argp);
        if (ret < 0) {
            BAFS_CTRL_ERR("IOCTL to dma map memory failed\n");
            goto out_release_ctrl;
        }
        break;
    default:
        ret                                     = -EINVAL;
        BAFS_CTRL_ERR("Invalid IOCTL cmd \t cmd = %u\n", cmd);
        goto out_release_ctrl;
        break;
    }

    ret = 0;
out_release_ctrl:
    bafs_ctrl_release(ctrl);
out:
    return ret;
}


static int
bafs_ctrl_open(struct inode* inode, struct file* file)
{
    int ret = 0;

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


static int
bafs_ctrl_file_release(struct inode* inode, struct file* file)
{
    int ret = 0;

    struct bafs_ctrl* ctrl = (struct bafs_ctrl*) file->private_data;

    if (!ctrl) {
        ret = -EINVAL;
        goto out;
    }
    bafs_ctrl_release(ctrl);
    return ret;
out:
    return ret;
}


int
bafs_ctrl_mmap(struct bafs_ctrl* ctrl, struct vm_area_struct* vma, const unsigned long vaddr, unsigned long* map_size)
{
    int ret = 0;

    if (!ctrl) {
        ret = -EINVAL;
        goto out;
    }
    bafs_get_ctrl(ctrl);
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    *map_size         = pci_resource_len(ctrl->pdev, 0);

    ret = io_remap_pfn_range(vma, vaddr, pci_resource_start(ctrl->pdev, 0), *map_size, vma->vm_page_prot);
    if (ret < 0) {
        goto out_put_ctrl;
    }

    return ret;
out_put_ctrl:
    bafs_ctrl_release(ctrl);
out:
    return ret;
}

static int
__bafs_ctrl_mmap(struct file* file, struct vm_area_struct* vma)
{
    int ret = 0;

    unsigned long map_size = 0;

    struct bafs_ctrl* ctrl = (struct bafs_ctrl*) file->private_data;

    if (!ctrl) {
        ret = -EINVAL;
        goto out;
    }
    ret = bafs_ctrl_mmap(ctrl, vma, vma->vm_start, &map_size);
    if (ret < 0) {
        goto out;
    }

    return ret;
out:
    return ret;
}

static const
struct file_operations bafs_ctrl_fops = {
    .owner          = THIS_MODULE,
    .open           = bafs_ctrl_open,
    .unlocked_ioctl = bafs_ctrl_ioctl,
    .release        = bafs_ctrl_file_release,
    .mmap           = __bafs_ctrl_mmap,

};

int
bafs_ctrl_alloc(struct bafs_ctrl ** out, struct pci_dev * pdev, int bafs_major,
                struct device * bafs_core_device)
{
    struct bafs_ctrl * ctrl;
    int ret;

    ctrl = kzalloc(sizeof(*ctrl), GFP_KERNEL);
    if(ctrl == NULL) {
        ret = -ENOMEM;
        goto out;
    }

    get_device(&pdev->dev);

    /* PCI Stuff */
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

    ctrl->pdev  = pdev;

    ctrl->major = MAJOR(bafs_major);

    ret = bafs_get_minor_number();
    if(ret < 0) {
        goto out_disable_pci_device;
    }
    ctrl->minor = ret;
 
    ret = ida_simple_get(&bafs_ctrl_ida, 0, 0, GFP_KERNEL);
    if(ret < 0) {
        goto out_minor_put;
    }
    ctrl->ctrl_id = ret;

    cdev_init(&ctrl->cdev, &bafs_ctrl_fops);
    ctrl->cdev.owner = THIS_MODULE;

    ret = cdev_add(&ctrl->cdev, MKDEV(ctrl->major, ctrl->minor), 1);
    if(ret < 0) {
        goto out_ctrl_id_put;
    }

    ctrl->core_dev = get_device(bafs_core_device);
    ctrl->device = device_create(bafs_ctrl_class, bafs_core_device,
                                 MKDEV(ctrl->major, ctrl->minor),
                                 ctrl, BAFS_CTRL_DEVICE_NAME, ctrl->ctrl_id);
    if(IS_ERR(ctrl->device)) {
        ret = PTR_ERR(ctrl->device);
        BAFS_CORE_ERR("Failed to create ctrl device \t err = %d\n", ret);
        goto out_cdev_del;
    }
    kref_init(&ctrl->ref);

    BAFS_CORE_DEBUG("Created ctrl with id %d major %d minor %d \t err = %d\n", ctrl->ctrl_id, ctrl->major, ctrl->minor, ret);

    *out = ctrl;
    return 0;

out_cdev_del:
    put_device(ctrl->core_dev);
    cdev_del(&ctrl->cdev);

out_ctrl_id_put:
    ida_simple_remove(&bafs_ctrl_ida, ctrl->ctrl_id);

out_minor_put:
    bafs_put_minor_number(ctrl->minor);

out_disable_pci_device:
    pci_disable_device(pdev);

out_release_pci_region:
    pci_release_region(pdev, 0);

out_clear_pci_drvdata:
    pci_set_drvdata(pdev, NULL);
    pci_clear_master(pdev);
    put_device(&pdev->dev);

    kfree(ctrl);
out:
    return ret;
}

