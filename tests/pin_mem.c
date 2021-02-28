#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <bafs.h>



int main(int argc, char* argv[] ) {
    int ret = 0;
    bafs_mem_hnd_t handle;
    unsigned size;
    unsigned loc;
    void* addr = NULL;
    if (argc < 2) {
        fprintf(stderr, "Please specify the memory size.\n");
        exit(EXIT_FAILURE);
    }

    size = strtoul(argv[1], NULL, 0);
    loc = BAFS_MEM_CPU;

    ret = bafs_core_reg_mem(size, loc, &handle);
    if (ret) {
        perror("Error while registering memory");
        exit(EXIT_FAILURE);
    }

    printf("Successfully registered memory with handle: %u\n", handle);

    ret = posix_memalign(&addr, 4096, size);
    if (ret) {
        perror("Unable to allocate cpu memory with posix_memalign");
        exit(EXIT_FAILURE);
    }




    ret = bafs_core_pin_mem(&addr, size, handle);

    if (ret) {
        perror("Error while pinning memory");
        exit(EXIT_FAILURE);
    }

    printf("Successfully pinned memory\n");


    return EXIT_SUCCESS;


}
