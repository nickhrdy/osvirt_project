/*
 * user.c - an application template (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */
#include <syscall.h>
#include<mm.h>

void user_start(void) {

    __syscall1(0, (long)"This is the first message from the user program!\n");

    //void* p = mm_malloc(32);
    memlib_init();
    __syscall0(2);

    //extend_heap(64);
    //__syscall1(0, (long)"This is the second message from the user program!\n");

    //__syscall0(2);

    void* a = mm_malloc(32);
    __syscall0(2);
    void* b = mm_malloc(32);
    void* c = mm_malloc(32);
    void* d = mm_malloc(32);
    mm_malloc(32);
    mm_malloc(32);
    mm_malloc(512);
    mm_malloc(65);

    //__syscall2(3, (long)"Malloced area starts at 0x%llx\n", (long)a);
    __syscall0(2);

    mm_free(b);
    __syscall0(2);
    mm_free(c);
    __syscall0(2);
    mm_free(a);
    __syscall0(2);
    mm_realloc(d, 64);
    __syscall0(2);


    /* Never exit */
    while (1) {};
}
