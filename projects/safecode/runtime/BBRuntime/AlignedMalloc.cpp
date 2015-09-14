#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "safecode/Runtime/BBMetaData.h"

/* On a dlmalloc/ptmalloc malloc implementation, memalign is performed by allocating
 * a block of size (alignment+size), and then finding the correctly aligned location
 * within that block, and try to give back the memory before the correctly aligned
 * location. This means that a memalign-based baggy bounds allocator can use up to
 * roughly 2x the amount the memory you'd expect.
 */

int next_pow_of_2(size_t size) {
  unsigned int i ;
  for (i = 1; i < size; i = i << 1) ;
  return (i < 16 ? 16 : i);
}

extern "C" void* malloc(size_t size) {
  size_t adjusted_size = size + sizeof(BBMetaData);
  size_t aligned_size = next_pow_of_2(adjusted_size);
  void *vp;
  posix_memalign(&vp, aligned_size, aligned_size);

  BBMetaData *data = (BBMetaData*)((uintptr_t)vp + aligned_size - sizeof(BBMetaData));
  data->size = size;
  data->pool = NULL;
  return vp;
}

extern "C" void* calloc(size_t nmemb, size_t size) {
  size_t aligned_size = next_pow_of_2(nmemb*size+sizeof(BBMetaData));
  void *vp;
  posix_memalign(&vp, aligned_size, aligned_size);
  memset(vp, 0, aligned_size);
  BBMetaData *data = (BBMetaData*)((uintptr_t)vp + aligned_size - sizeof(BBMetaData));
  data->size = nmemb*size;
  data->pool = NULL;
  return vp;
}

extern "C" void* realloc(void *ptr, size_t size) {
  if (ptr == NULL) {
    return malloc(size);
  }

  size += sizeof(BBMetaData);
  size_t aligned_size = next_pow_of_2(size);
  void *vp;
  posix_memalign(&vp, aligned_size, aligned_size);
  memcpy(vp, ptr, size);
  free(ptr);
  BBMetaData *data = (BBMetaData*)((uintptr_t)vp + aligned_size - sizeof(BBMetaData));
  data->size = size;
  data->pool = NULL;
  return vp;
}
