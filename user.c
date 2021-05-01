/*
 * user.c - an application template (Assignment 2, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */
#include <syscall.h>
#include<mm.h>

void user_start(void) {

    __syscall1(0, (long)"This is the first message from the user program!\n");
    __syscall1(0, (long)"This is the second message from the user program!\n");

    memlib_init();

    /* Never exit */
    while (1) {};
}
