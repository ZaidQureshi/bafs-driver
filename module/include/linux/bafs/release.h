#ifndef _LINUX_BAFS_RELEASE_H_
#define _LINUX_BAFS_RELEASE_H_


#include "util.h"
#include "types.h"


extern dev_t bafs_major;

extern struct class* bafs_ctrl_class;
extern struct class* bafs_group_class;

extern struct ida bafs_minor_ida;
extern struct ida bafs_ctrl_ida;
extern struct ida bafs_group_ida;


static inline
void __bafs_group_release(struct kref* ref) {
    int j = 0;

    struct bafs_group* group;


    group = container_of(ref, struct bafs_group, ref);
    BAFS_GROUP_DEBUG("Removing GROUP \t group: %p\n", group);
    if (group) {
        spin_lock(&group->lock);
        device_destroy(bafs_group_class, MKDEV(MAJOR(bafs_major), group->minor));
        for (j = 0; j < group->n_ctrls; j++) {
            bafs_put_ctrl(group->ctrls[j]);
        }

        kfree(group->ctrls);

        put_device(group->core_dev);
        cdev_del(&group->cdev);
        ida_simple_remove(&bafs_group_ida, group->group_id);
        ida_simple_remove(&bafs_minor_ida, group->minor);
        spin_unlock(&group->lock);
        BAFS_CTRL_DEBUG("Removed GROUP \t group: %p\n", group);

        kfree(group);



    }
}


#endif // __BAFS_CTRL_RELEASE_H__
