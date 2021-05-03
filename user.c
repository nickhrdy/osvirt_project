/*
 * user.c - an application template (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */
#include <syscall.h>
#include<mm.h>

#define SHOW_HEAP() __syscall0(2)
#define MALLOC(sz) mm_malloc(sz); SHOW_HEAP()
#define FREE(ptr) mm_free(ptr); SHOW_HEAP()
#define REALLOC(ptr, sz) mm_realloc(ptr, sz); SHOW_HEAP()


void eval_malloc(void){
    memlib_init();
    size_t list_size = 1000;

    __syscall1(4, (long)"Correctness");
    for(size_t i = 0; i < 3; i++){
        void * a = mm_malloc(256);
        void * b = mm_malloc(256);
        b = mm_realloc(b, 4096);
        a = mm_realloc(a, 1024);
        b = mm_realloc(b, 1024);
        a = mm_realloc(a, 4096);
        mm_free(b);
        mm_free(a);
    }
    __syscall1(4, (long)"Correctness");
    debug_heap_user();

    __syscall1(4, (long)"Big allocate");
    for(size_t i = 0; i < list_size; i++){
         mm_malloc(256);
    }
    for(size_t i = 0; i < list_size; i++){
         mm_malloc(1024);
    }
    for(size_t i = 0; i < list_size; i++){
        mm_malloc(4096);
    }
     __syscall1(4, (long)"Big allocate");
    //debug_heap_user();
}

void user_start(void) {

    eval_malloc();



    // __syscall1(0, (long)"This is the first message from the user program!\n");

    // //void* p = mm_malloc(32);
    // memlib_init();

    // SHOW_HEAP();

    // void* a = MALLOC(32);
    // void* b = MALLOC(32);
    // void* c = MALLOC(32);
    // void* d = MALLOC(32);
    // MALLOC(32);
    // MALLOC(32);
    // MALLOC(512);
    // MALLOC(65);

    // FREE(b);
    // FREE(c);
    // FREE(a);

    // REALLOC(d, 64);

    /* Never exit */
    while (1) {};
}
