#pragma once

#define PAGESIZE 4096ULL
#define PAGESHIFT 12ULL

#include <list.h>

/* page frame attributes */
typedef struct page_info {
    uint8_t inuse : 1;
    uint8_t reserved : 1;
    uint8_t __padding : 6;  // don't access this
} page_info_t;

/* page table information block */
typedef struct page_properties {
    size_t size;
    page_info_t* info_buffer;
} page_properties_t;

typedef struct buddy_block {
    //start + size
    uint64_t physical_addr; //physical addr of the start of this block
    size_t size; //size of this buddy block in bytes; 0 when not in use
    list_elem_t elem;
} buddy_block_t;

uint64_t get_memory_map_size(efi_memory_descriptor_t* memory_map,
                             uint64_t memory_map_size,
                             uint64_t memory_map_desc_size);

int init_page_properties(efi_memory_descriptor_t* memory_map,
                         uint64_t memory_map_size,
                         uint64_t memory_map_desc_size);

int alloc_page(void* addr);

int alloc_pages(void* addr, size_t num_pages);

int free_page(void* addr);

int free_pages(void* addr, size_t num_pages);

void* request_page();

void clear_page(void* addr);

void print_allocator();

void print_available_memory();
uint64_t get_largest_segment_size(efi_memory_descriptor_t* memory_map, uint64_t memory_map_size, uint64_t memory_map_desc_size);

void* get_block(size_t num_pages);

void free_block(void* addr);

void debug_buddy_lists();