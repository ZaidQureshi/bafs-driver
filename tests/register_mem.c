#include <stdio.h>
#include <stdlib.h>

#include <bafs.h>



int main(int argc, char* argv[] ) {
    int ret = 0;
    bafs_mem_hnd_t handle;
    struct bafs_ctrl_t ctrl_handle;
    const char* ctrl_name;
    unsigned size;
    unsigned loc;
    if (argc < 4) {
        fprintf(stderr, "Please specify the ctrl, memory size and location.\n");
        exit(EXIT_FAILURE);

    }
    ctrl_name = argv[1];
    size = strtoul(argv[2], NULL, 0);

    loc = strtoul(argv[3], NULL, 0);


    ret = bafs_ctrl_open(ctrl_name, &ctrl_handle);
    if (ret) {
        perror("Error while openning ctrl");
        exit(EXIT_FAILURE);
    }

    printf("Successfully opened ctrl file\n");


    ret = bafs_ctrl_reg_mem(size, loc, &ctrl_handle, &handle);
    if (ret) {
        perror("Error while registering memory");
        exit(EXIT_FAILURE);
    }

    printf("Successfully registered memory with handle: %u\n", handle);

    return EXIT_SUCCESS;


}
