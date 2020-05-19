#include <cuda.h>
#include <cuda_device_runtime_api.h>
#include <cuda_runtime.h>
#include <iostream>
#include <limits>

/**
 * This is a utility funstion for checking CUDA Errors,
 * NOTE: This function was taken from a blog www.beechwood.eu
 * @param err
 * @param file
 * @param line
 */
static void HandleError(cudaError_t err,
                        const char *file,
                        int line) {
  if (err != cudaSuccess) {
    int aa = 0;
    printf("%s in %s at line %d\n", cudaGetErrorString(err),
           file, line);
    scanf("%d", &aa);
    exit(EXIT_FAILURE);
  }
}
#define HANDLE_ERROR(err) (HandleError( err, __FILE__, __LINE__ ))

int main() {
  int64_t length;
  length = 1000000;
  int64_t size = length * sizeof(int64_t);

  int64_t *array = new int64_t[length];

  auto max_limit = std::numeric_limits<int32_t>::max();
  srand(time(0));

  for (int64_t i = 0; i < length; i++) {
    int32_t val_1 = rand() % max_limit;
    array[i] = val_1;
  }

  int64_t *d_array;
  HANDLE_ERROR(cudaMalloc((void **) &d_array, size));
  HANDLE_ERROR(cudaMemcpy(d_array, array, size, cudaMemcpyHostToDevice));

  // Experiment1: Does `cuPointerGetAttribute()` work for normal GPU and CPU pointers
  cudaPointerAttributes attr;
  cudaPointerAttributes att_1;
  HANDLE_ERROR(cudaPointerGetAttributes(&attr, (void *) d_array));

  // This returns an error, CUDA 11 promises to mitigate this by telling that host pointers are  cudaMemoryTypeUnregistered instead of throwing an error
  // This is useful, since now we know that if there's an error it's probably because the pointer is in Main Memory
//  HANDLE_ERROR(cudaPointerGetAttributes(&att_1, (void*) array));
  std::cout << attr.device << "\n";
//  std::cout << att_1.type << "\n";

}