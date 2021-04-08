#pragma once

typedef signed long long ssize_t;
typedef unsigned long long size_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define SIZE_MAX	0xFFFFFFFFFFFFFFFFULL
typedef long long intptr_t;
typedef unsigned long long uintptr_t;

#define NULL ((void *) 0)



struct gate_descriptor {
    uint64_t gd_looffset:16;
    uint64_t gd_selector:16;    /* gate segment selector */
    uint64_t gd_ist:3;          /* IST select */
    uint64_t gd_xx1:5;          /* reserved */
    uint64_t gd_type:5;         /* segment type */
    uint64_t gd_dpl:2;          /* segment descriptor priority level */
    uint64_t gd_p:1;            /* segment descriptor present */
    uint64_t gd_hioffset:48;    /* gate offset (msb) */
    uint64_t gd_xx2:8;          /* reserved */
    uint64_t gd_zero:5;         /* must be zero */
    uint64_t gd_xx3:19;         /* reserved */
}__attribute__((__packed__));

typedef struct gate_descriptor gate_descriptor_t; /* IDT entry */

struct tls_block 
{
    struct tls_block *myself;
    char padding[4096-8];
};

typedef struct tls_block tls_block_t;