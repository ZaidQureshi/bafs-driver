#include <stdio.h>
#include <stdlib.h>

#include <bafs.h>



int main(int argc, char* argv[] ) {
    int ret = 0;
    bafs_mem_hnd_t handle;
    unsigned size;
    unsigned loc;
    if (argc < 3) {
        fprintf(stderr, "Please specify the memory size and location.\n");
        exit(EXIT_FAILURE);
    }

    size = strtoul(argv[1], NULL, 0);
    loc = strtoul(argv[2], NULL, 0);

    ret = bafs_core_reg_mem(size, loc, &handle);
    if (ret) {
        perror("Error while registering memory");
        exit(EXIT_FAILURE);
    }

    printf("Successfully registered memory with handle: %u\n", handle);

    return EXIT_SUCCESS;


}
