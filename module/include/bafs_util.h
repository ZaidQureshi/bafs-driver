#ifndef __BAFS_UTIL_H__
#define __BAFS_UTIL_H__

#define BAFS_MINORS     (1U << MINORBITS)

#define                      BAFS_CORE_DEVICE_NAME "bafs"
#define BAFS_CORE_CLASS_NAME                       "bafs"

#define BAFS_CTRL_DEVICE_NAME "bafsc%d"
#define BAFS_CTRL_CLASS_NAME  "bafsc"

#define BAFS_GROUP_DEVICE_NAME "bafsg%d"
#define BAFS_GROUP_CLASS_NAME  "bafsg"



#define UNUSED(x) (void)(x)

#define BAFS_MSG(LEVEL, DEV_CLASS, FMT, ...)    \
    printk(LEVEL DEV_CLASS ": "  FMT, ##__VA_ARGS__)


static int debug = 1;
#define BAFS_DEBUG(DEV_CLASS, FMT, ...)         \
    do {                                        \
        if (debug)                              \
            BAFS_MSG(KERN_DEBUG, DEV_CLASS, FMT, ##__VA_ARGS__); \
    } while (0)

#define BAFS_CORE_DEBUG(FMT, ...)               \
    BAFS_DEBUG(BAFS_CORE_CLASS_NAME, FMT, ##__VA_ARGS__)

#define BAFS_CTRL_DEBUG(FMT, ...)                 \
    BAFS_DEBUG(BAFS_CTRL_CLASS_NAME, FMT, ##__VA_ARGS__)

#define BAFS_GROUP_DEBUG(FMT, ...)                \
    BAFS_DEBUG(BAFS_GROUP_CLASS_NAME, FMT, ##__VA_ARGS__)


static int info = 1;
#define BAFS_INFO(DEV_CLASS, FMT, ...)          \
    do {                                        \
        if (info)                               \
            BAFS_MSG(KERN_INFO, DEV_CLASS, FMT, ##__VA_ARGS__); \
    } while (0)

#define BAFS_CORE_INFO(FMT, ...)                \
    BAFS_INFO(BAFS_CORE_CLASS_NAME, FMT, ##__VA_ARGS__)

#define BAFS_CTRL_INFO(FMT, ...)                \
    BAFS_INFO(BAFS_CTRL_CLASS_NAME, FMT, ##__VA_ARGS__)

#define BAFS_GROUP_INFO(FMT, ...)               \
    BAFS_INFO(BAFS_GROUP_CLASS_NAME, FMT, ##__VA_ARGS__)


#define BAFS_MSG_ERR(LEVEL, DEV_CLASS, FMT, ...)    \
    printk(LEVEL DEV_CLASS " :%s:%d:%s(): " FMT, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define BAFS_ERR(DEV_CLASS, FMT, ...)           \
    BAFS_MSG_ERR(KERN_ERR, DEV_CLASS, FMT, ##__VA_ARGS__);

#define BAFS_CORE_ERR(FMT, ...)                 \
    BAFS_ERR(BAFS_CORE_CLASS_NAME, FMT, ##__VA_ARGS__)

#define BAFS_CTRL_ERR(FMT, ...)                 \
    BAFS_ERR(BAFS_CTRL_CLASS_NAME, FMT, ##__VA_ARGS__)

#define BAFS_GROUP_ERR(FMT, ...)                 \
    BAFS_ERR(BAFS_GROUP_CLASS_NAME, FMT, ##__VA_ARGS__)


#endif                          // __BAFS_UTIL_H__
