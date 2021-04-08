/*
 * user.c - an application template (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */
#include <syscall.h>

__thread int a[100];

void user_start(void) {

    for(int i = 0; i < 100; i++) { a[i] = i; }

    __syscall1(0, (long)"This is the first message from the user program!\n");
    __syscall1(0, (long)"This is the second message from the user program!\n");

    /* Never exit */
    while (1) {};
}
