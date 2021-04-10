#pragma once

#define PAGESIZE 4096ULL
#define PAGESHIFT 12ULL

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

/* information about all page frames */
extern page_properties_t* properties_ptr;

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