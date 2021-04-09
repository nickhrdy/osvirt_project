#pragma once

#define PAGESIZE 4096
#define PAGESHIFT 12

typedef struct page_info {
    uint8_t inuse:1;
    uint8_t reserved:1;
    uint8_t __padding:6; //don't access this
} page_info_t;

typedef struct page_properties {
    size_t size;
    page_info_t* info_buffer;
} page_properties_t;

extern page_properties_t* properties_ptr;

int init_page_properties(efi_memory_descriptor_t* memory_map, uint64_t memory_map_size, uint64_t memory_map_desc_size);

int alloc_page(void* addr);

int alloc_pages(void* addr, size_t num_pages);

int free_page(void* addr);

int free_pages(void* addr, size_t num_pages);

void* request_page();

void print_allocator();