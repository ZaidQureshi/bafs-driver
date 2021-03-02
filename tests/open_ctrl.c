#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <bafs.h>

#define PAGE_SIZE 4096


int main(int argc, char* argv[] ) {
    const char* ctrl_name;

    struct bafs_ctrl_t ctrl_handle;

    if (argc < 2) {
        fprintf(stderr, "Please specify the controller.\n");
        exit(EXIT_FAILURE);
    }

    ctrl_name = argv[1];


    ret = bafs_ctrl_open(ctrl_name, &ctrl_handle);
    if (ret) {
        perror("Error while openning ctrl");
        exit(EXIT_FAILURE);
    }

    printf("Successfully opened controller \n");



    return EXIT_SUCCESS;


}
