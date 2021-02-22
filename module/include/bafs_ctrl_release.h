#ifndef __BAFS_CTRL_RELEASE_H__
#define __BAFS_CTRL_RELEASE_H__


#include "bafs_util.h"
#include "bafs_types.h"


extern dev_t bafs_major;

extern struct class* bafs_ctrl_class;

extern struct ida bafs_minor_ida;
extern struct ida bafs_ctrl_ida;

static void __bafs_ctrl_release(struct kref* ref) {
    struct bafs_ctrl* ctrl;
    struct bafs_group* group;

    ctrl = container_of(ref, struct bafs_ctrl, ref);
    if (ctrl) {
        group = ctrl->group;
        if (group) {
            spin_lock(&group->lock);
            list_del(&ctrl->group_list);
            spin_unlock(&group->lock);

        }
        device_destroy(bafs_ctrl_class, MKDEV(MAJOR(bafs_major), ctrl->minor));

        cdev_del(&ctrl->cdev);
        ida_simple_remove(&bafs_ctrl_ida, ctrl->ctrl_id);
        ida_simple_remove(&bafs_minor_ida, ctrl->minor);
        pci_disable_device(ctrl->pdev);
        pci_release_region(ctrl->pdev, 0);
        pci_clear_master(ctrl->pdev);

        pci_set_drvdata(ctrl->pdev, NULL);

        kfree_rcu(ctrl, rh);
        kref_put(&group->ref, NULL);


    }
}


#endif // __BAFS_CTRL_RELEASE_H__
