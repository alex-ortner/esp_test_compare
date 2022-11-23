
#ifndef __DYNAMIC_MALLOC__
#define __DYNAMIC_MALLOC__

#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

__ptr_t n_realloc(__ptr_t __ptr, size_t __size, size_t __n){
  return realloc(__ptr, __size * __n);
}

void** n_realloc_array(void ** __ptr, size_t __size, size_t __inner_size, size_t __n, size_t __old_n){
  void ** __tmp_ptr = (void **)realloc(__ptr, __size * __n);

  for(size_t i = __old_n; i < __n; i++){
    __tmp_ptr[i] = malloc(__inner_size);
  }
  return __tmp_ptr;
}

void** n_malloc_array(size_t __size, size_t __inner_size, size_t __n){
  void** __tmp_ptr = malloc(__size * __n);
//  printf("New pointer created %p\n", (void *)__tmp_ptr);
  if(__tmp_ptr == NULL) return NULL;
  for(size_t i = 0; i < __n; i++){
    __tmp_ptr[i] = malloc(__inner_size);
  }
  return __tmp_ptr;
}

void free_array(void ** __ptr, size_t __n){
  for(size_t i = 0; i < __n; i++){
    free(__ptr[i]);
  }
  free(__ptr);
}

#endif
