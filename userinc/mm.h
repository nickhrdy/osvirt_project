#pragma once

#include <types.h>
#include <syscall.h>

int mm_init(void);
void * mm_malloc(size_t size);
void * mm_realloc(void *addr, size_t size);
void mm_free(void *addr);

#define MAX_HEAP (1024*(1<<15))  /* 32 MB */
#define MIN_BLOCK_SIZE 16 /*minimum block size in bytes*/
#define max(a,b) a >= b ? a : b
#define min(a,b) a >= b ? b : a


void memlib_init(void);


typedef struct boundary_block{
    size_t free:1;
    size_t size:63;
}boundary_block_t;
boundary_block_t* extend_heap(size_t size);