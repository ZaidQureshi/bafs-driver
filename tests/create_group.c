#include <stdio.h>
#include <stdlib.h>

#include <bafs.h>



int main(int argc, char* argv[] ) {
    int ret = 0;
    unsigned int n_ctrls = argc - 1;
    char group_name[30];
    if (argc < 2) {
        fprintf(stderr, "Please specify at least one controller to make a group from.\n");
        exit(EXIT_FAILURE);
    }

    printf("n_ctrls: %u\n", n_ctrls);

    ret = bafs_core_create_group(n_ctrls, &argv[1], group_name);
    if (ret) {
        perror("Error while creating group");
        exit(EXIT_FAILURE);
    }

    printf("Successfully created %s group\n", group_name);

    return EXIT_SUCCESS;


}
