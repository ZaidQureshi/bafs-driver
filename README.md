# bafs-driver

To compile this driver you need CMAKE 3.18 or higher. The recommended compilation flow is
```
$ mkdir build
$ cd build
$ ../configure
$ make -j
```

Currently you **MUST** have the NVIDIA Linux kernel driver installed in your system. The build script should be able to detect where this driver is located and compile it if necessary.
