#include <stdio.h>
#include <stdlib.h>

#include <bafs.h>



int main(int argc, char* argv[] ) {
    int ret = 0;
    if (argc < 2) {
        fprintf(stderr, "Please specify the group device to destroy.\n");
        exit(EXIT_FAILURE);
    }

    ret = bafs_core_delete_group(argv[1]);
    if (ret) {
        perror("Error while deleting group");
        exit(EXIT_FAILURE);
    }

    printf("Successfully deleted %s group\n", argv[1]);

    return EXIT_SUCCESS;


}
