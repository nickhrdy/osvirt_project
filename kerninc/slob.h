#pragma once

#include <printf.h>
#include <allocator.h>
#include <list.h>
#include <halt.h>
#include <types.h>


typedef enum slob_type {SMALL, MEDIUM, LARGE} slob_type_t;

typedef struct slob_blk {
  list_elem_t elem;
  slob_type_t size;
  char padding[48];
} slob_blk_t;

//allocates memory through slab allocator.
void *kmalloc(size_t size);

//allocates memory (and zeroes it out like calloc() in libc) through the slab allocator.
void *kzalloc(size_t size);

//resize existing allocation.
void * krealloc(void *addr, size_t size);

//frees memory previously allocated.
void kfree(void *addr);

//frees memory previously allocated and zeros out space.
void kzfree(void *addr);

//print all elements of all slob lists
void debug_slob_lists();

//initialize slob lists
void slob_init(size_t num_pages);

//alloc slob. Should be removed from .h once tested
void *__slob_alloc(size_t size);

//print number element from all slob lists
void slob_list_counts();