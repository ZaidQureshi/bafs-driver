NVIDIA_DRIVER_VER ?= $(shell nvidia-smi --query-gpu=driver_version --format=csv,noheader | head -1)

NVIDIA_DRIVER_DIR ?= /usr/src/nvidia-$(NVIDIA_DRIVER_VER)
NVIDIA_INCLUDE_DIR ?= /usr/src/nvidia-$(NVIDIA_DRIVER_VER)/nvidia

obj-m := bafs_core.o

ccflags-y += -I$(NVIDIA_INCLUDE_DIR)

KBUILD_EXTRA_SYMBOLS := $(NVIDIA_DRIVER_DIR)/Module.symvers
