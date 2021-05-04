/*
 * user.c - an application template (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */
#include <syscall.h>
#include<mm.h>

// macros for debugging
#define SHOW_HEAP() __syscall0(2)
#define MALLOC(sz) mm_malloc(sz); SHOW_HEAP()
#define FREE(ptr) mm_free(ptr); SHOW_HEAP()
#define REALLOC(ptr, sz) mm_realloc(ptr, sz); SHOW_HEAP()

void malloc_test(void){
    __syscall1(0, (long)"\nMallocing two chunks of 256 bytes...\n");
    void * a = mm_malloc(256);
    void * b = mm_malloc(256);
    debug_heap_user();

    __syscall1(0, (long)"\nReallocing one chunk to 4096 bytes...\n");
    b = mm_realloc(b, 4096);
    debug_heap_user();

    __syscall1(0, (long)"\nFreeing both chunks...\n");
    mm_free(b);
    mm_free(a);
    debug_heap_user();
}

void user_start(void) {
    __syscall1(0, (long)"\n\n---USER---\n\n");
    malloc_test();

    __syscall1(0, (long)"Reached the end of the user program!\n");
    while(1){};
}
