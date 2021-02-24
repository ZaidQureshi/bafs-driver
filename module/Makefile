ifneq ($(KERNELRELEASE),)

include Kbuild

else

KDIR ?= /lib/modules/`uname -r`/build


all: module

module:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	rm -rf *.o .*.o.d *.a *.ko* *.mod.* .*.cmd Module.symvers modules.order .tmp_versions/ *~ core .depend TAGS .cache.mk

load:
	insmod bafs_core.ko

unload:
	rmmod bafs_core.ko

reload:
	rmmod bafs_core.ko
	insmod bafs_core.ko

endif
