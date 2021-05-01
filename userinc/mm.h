#pragma once

#include <types.h>
#include <syscall.h>

int mm_init(void);
void * mm_malloc(size_t size);
void * mm_realloc(void *addr, size_t size);
void mm_free(void *addr);

#define MAX_HEAP (1024*(1<<20))  /* 1024 MB */

void memlib_init(void);