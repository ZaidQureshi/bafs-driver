#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <bafs.h>

#define PAGE_SIZE 4096


int main(int argc, char* argv[] ) {
    int ret = 0;
    unsigned size;
    unsigned loc;
    void* addr = NULL;
    int n_pages;
    const char* ctrl_name;
    struct bafs_dma_t dma_handle;

    struct bafs_ctrl_t ctrl_handle;

    if (argc < 3) {
        fprintf(stderr, "Please specify the memory size and controller.\n");
        exit(EXIT_FAILURE);
    }

    size = strtoul(argv[1], NULL, 0);
    ctrl_name = argv[2];
    loc = BAFS_MEM_CPU;



    ret = posix_memalign(&addr, 4096, size);
    if (ret) {
        perror("Unable to allocate cpu memory with posix_memalign");
        exit(EXIT_FAILURE);
    }

    ret = bafs_ctrl_open(ctrl_name, &ctrl_handle);
    if (ret) {
        perror("Error while openning ctrl");
        exit(EXIT_FAILURE);
    }

    printf("Successfully opened ctrl file\n");

    ret = bafs_ctrl_map((void**)&addr, size, loc, &ctrl_handle);
    if (ret) {
        perror("Error while pinning memory");
        exit(EXIT_FAILURE);
    }

    printf("Successfully registered and pinned memory\n");




    n_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;


    dma_handle.dma_addrs = malloc(sizeof(void*) * n_pages);

    if (dma_handle.dma_addrs == NULL) {
        perror("Error allocating dma addresses");
        exit(EXIT_FAILURE);
    }


    dma_handle.n_dma_addrs = n_pages;

    ret = bafs_ctrl_dma_map_mem(addr, &dma_handle, &ctrl_handle);
    if (ret) {
        perror("Error while dma mapping memory");
        exit(EXIT_FAILURE);
    }



    return EXIT_SUCCESS;


}
