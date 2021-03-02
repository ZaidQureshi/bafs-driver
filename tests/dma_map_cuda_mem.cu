#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <bafs.h>

#include <cuda.h>

#define PAGE_SIZE 65536

#define ALIGN_SIZE(x) (x + PAGE_SIZE)
#define ALIGN_ADDR(a) ((a + PAGE_SIZE) & ~(PAGE_SIZE - 1))

int main(int argc, char* argv[] ) {
    //CUresult cu_ret;
    //CUdevice cu_device;
    //CUcontext cu_ctx;
    int ret = 0;
    unsigned gpu_id;
    unsigned orig_size;
    unsigned aligned_size;
    unsigned loc;
    cudaError_t crt;
    //CUdeviceptr addr = 0;
    //CUdeviceptr aligned_addr = 0;
    void* addr;
    void* aligned_addr;
    const char* ctrl_name;
    struct bafs_dma_t dma_handle;

    struct bafs_ctrl_t ctrl_handle;

    if (argc < 4) {
        fprintf(stderr, "Please specify the memory size, controller, and gpu id.\n");
        exit(EXIT_FAILURE);
    }

    orig_size = strtoul(argv[1], NULL, 0);

    aligned_size = ALIGN_SIZE(orig_size);
    ctrl_name = argv[2];
    gpu_id = strtoul(argv[3], NULL, 0);
    loc = BAFS_MEM_CUDA;

    // cu_ret = cuInit(0);
    // if (cu_ret != CUDA_SUCCESS) {
    //     perror("Unable to init cu driver");
    //     goto out_err;
    // }

    // cu_ret = cuDeviceGet(&cu_device, gpu_id);
    // if (cu_ret != CUDA_SUCCESS) {
    //     perror("Unable to get cu device");
    //     goto out_err;
    // }

    // cu_ret = cuCtxCreate(&cu_ctx, 0, cu_device);
    // if (cu_ret != CUDA_SUCCESS) {
    //     perror("Unable to create cu ctx");
    //     goto out_err;
    // }

    /* cu_ret = cuMemAlloc(&addr, aligned_size); */
    /* if (cu_ret != CUDA_SUCCESS) { */
    /*     perror("Unable to create cu ctx"); */
    /*     goto out_destroy_ctx; */
    /* } */

    crt = cudaMalloc(&addr, aligned_size);
    if (crt != cudaSuccess) {
        perror("cudaMalloc failed\n");
        goto out_destroy_ctx;
    }

    aligned_addr = addr;//(void*)ALIGN_ADDR(((unsigned long)addr));

    printf("orig_addr: %llx\taligned_addr: %llx\n", addr, aligned_addr);


    ret = bafs_core_map((void**)&aligned_addr, orig_size, loc);

    if (ret) {
        perror("Error while pinning memory");
        goto out_free_mem;
    }

    printf("Successfully registered and pinned memory\n");


    ret = bafs_ctrl_open(ctrl_name, &ctrl_handle);
    if (ret) {
        perror("Error while openning ctrl");
        goto out_free_mem;
    }

    printf("Successfully registered and pinned memory\n");

    dma_handle.dma_addrs = (void**)malloc(sizeof(void*) * ((orig_size + PAGE_SIZE - 1) / PAGE_SIZE));
    if (dma_handle.dma_addrs == NULL) {
        perror("Error allocating dma addresses");
        goto out_free_mem;
    }

    ret = bafs_ctrl_dma_map_mem((void*)aligned_addr, &dma_handle, &ctrl_handle);
    if (ret) {
        perror("Error while dma mapping memory");
        goto out_free_mem;
    }

    printf("Successfully dma mapped cpu memory\n");



    return EXIT_SUCCESS;

out_free_mem:
    cudaFree (addr);
out_destroy_ctx:
    //cuCtxDestroy(cu_ctx);
out_err:
    exit(EXIT_FAILURE);

}
