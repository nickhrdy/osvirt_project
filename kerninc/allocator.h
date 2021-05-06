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

/* Data for each buddy block, stored in double linked lists*/
typedef struct buddy_block {
    //start + size
    uint64_t physical_addr; //physical addr of the start of this block
    size_t size; //size of this buddy block in bytes; 0 when not in use
    list_elem_t elem;
} buddy_block_t;

/* Walk the memory map and sum the segments to get the total memory map size*/
uint64_t get_memory_map_size(efi_memory_descriptor_t* memory_map,
                             uint64_t memory_map_size,
                             uint64_t memory_map_desc_size);

/*Find the largest segment of free physical memory */          
uint64_t get_largest_segment_size(efi_memory_descriptor_t* memory_map, uint64_t memory_map_size, uint64_t memory_map_desc_size);

/*Init for both the naive page frame allocator and the buddy list*/
int init_page_properties(efi_memory_descriptor_t* memory_map,
                         uint64_t memory_map_size,
                         uint64_t memory_map_desc_size);

/*Allocate pages using the naive page frame allocator*/
int alloc_page(void* addr);
int alloc_pages(void* addr, size_t num_pages);

/*Free pages using the naive page frame allocator*/
int free_page(void* addr);
int free_pages(void* addr, size_t num_pages);

/*Allocate the first free page using the naive page frame allocator*/
void* request_page();

/*Set the contents of a page to 0*/
void clear_page(void* addr);

/*ask the buddy system for num_pages*/
void* get_block(size_t num_pages);

/*free for the buddy system at the given address*/
void free_block(void* addr);

/*Debugging print outs for naive and buddy allocator*/
void print_available_memory();
void print_allocator();
void debug_buddy_lists();