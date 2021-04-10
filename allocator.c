#include <types.h>
#include <allocator.h>
#include <printf.h>

static int __reserve_page(void*addr);
static int __reserve_pages(void* addr, size_t num_pages);
static int __unreserve_page(void*addr);
static int __unreserve_pages(void* addr, size_t num_pages);

page_properties_t* properties_ptr = NULL;
static int free_memory = 0;
static int used_memory = 0;

/* Unreserve a page. Return 0 on success.*/
static int __unreserve_page(void* addr){
    uint64_t idx = (uint64_t)addr >> PAGESHIFT;
    if(!properties_ptr->info_buffer[idx].reserved)
        return 1; //already unreserved
    properties_ptr->info_buffer[idx].reserved = 0;
    free_memory += PAGESIZE;
    used_memory -= PAGESIZE;
    return 0;
}

static int __unreserve_pages(void* addr, size_t num_pages){
    for(size_t i = 0 ; i < num_pages; i++){
        __unreserve_page((void*)((uint64_t)addr + (i * PAGESIZE)));
    }
    return 0;
}

/* Reserve a page. Return 0 on success.*/
static int __reserve_page(void* addr){
    uint64_t idx = (uint64_t)addr >> PAGESHIFT;
    if(properties_ptr->info_buffer[idx].reserved)
        return 1; //already reserved
    properties_ptr->info_buffer[idx].reserved = 1;
    free_memory -= PAGESIZE;
    used_memory += PAGESIZE;
    return 0;
}

/* Reserve a page. Return 0 on success.*/
static int __reserve_pages(void* addr, size_t num_pages){
    for(size_t i = 0 ; i < num_pages; i++){
        if(__reserve_page((void*)((uint64_t)addr + (i * PAGESIZE)))){
            __unreserve_pages(addr, i);
            return 1;
        }
    }
    return 0;
}

uint64_t get_memory_map_size(efi_memory_descriptor_t* memory_map, uint64_t memory_map_size, uint64_t memory_map_desc_size){
    static uint64_t total_size = 0;

    if (total_size > 0) return total_size;

    uint64_t num_map_entries = memory_map_size / memory_map_desc_size;
    for (int i = 0 ; i < num_map_entries; i++){
        efi_memory_descriptor_t* desc = (efi_memory_descriptor_t*)((uint64_t)memory_map + (i * memory_map_desc_size));
        total_size += desc->num_pages * PAGESIZE;
    }

    return total_size;
}

void reserve_special_segments(efi_memory_descriptor_t* memory_map, uint64_t memory_map_size, uint64_t memory_map_desc_size){
    uint64_t num_map_entries = memory_map_size / memory_map_desc_size;
    for (int i = 0 ; i < num_map_entries; i++){
        efi_memory_descriptor_t* desc = (efi_memory_descriptor_t*)((uint64_t)memory_map + (i * memory_map_desc_size));

        if (desc->type == EFI_RESERVED_MEMORY_TYPE ||
            desc->type == EFI_RUNTIME_SERVICES_CODE ||
            desc->type == EFI_RUNTIME_SERVICES_DATA ||
            desc->type == EFI_UNUSABLE_MEMORY ||
            desc->type == EFI_ACPI_RECLAIM_MEMORY ||
            desc->type == EFI_ACPI_MEMORY_NVS ||
            desc->type == EFI_MEMORY_MAPPED_IO ||
            desc->type == EFI_MEMORY_MAPPED_IO_PORT_SPACE ||
            desc->type == EFI_PAL_CODE){
                __reserve_pages(desc->physical_addr, desc->num_pages);
            }
    }
}


//give the page properties a space to initialize
int init_page_properties(efi_memory_descriptor_t* memory_map, uint64_t memory_map_size, uint64_t memory_map_desc_size){
    //find the largest free segment + its size
    void* addr = NULL;
    uint64_t largest = 0;
    size_t bitmap_size = 0;

    for (uint64_t i = 0; i < memory_map_size / memory_map_desc_size; i++){
        efi_memory_descriptor_t* desc = (efi_memory_descriptor_t*)((uint64_t)memory_map + (i * memory_map_desc_size));
        if (desc->type == EFI_CONVENTIONAL_MEMORY && desc->num_pages * PAGESIZE > largest){
            addr = desc->physical_addr;
            largest = desc->num_pages * PAGESIZE;
        }
    }

    //caculate the pages to be occupied by the allocator page info
    printf("Here is the size of our memory: %llu\n", get_memory_map_size(memory_map, memory_map_size, memory_map_desc_size));
    uint64_t num_total_mem_pages = get_memory_map_size(memory_map, memory_map_size, memory_map_desc_size) / PAGESIZE;
    properties_ptr->size = num_total_mem_pages;
    properties_ptr->info_buffer = (page_info_t*)addr;
    __builtin_memset(properties_ptr->info_buffer, 0, sizeof(page_info_t) * bitmap_size);

    //set sizes
    free_memory = get_memory_map_size(memory_map, memory_map_size, memory_map_desc_size);
    used_memory = 0;
    __reserve_page(0);
    __reserve_pages(addr, properties_ptr->size * 8 / PAGESIZE + 1); //reserve bitmap
    reserve_special_segments(memory_map, memory_map_size, memory_map_desc_size);
    //allocate the pages used by bitmap

    return 0;
}

/* Allocate a page. Return 0 on success.*/
int alloc_page(void* addr){
    uint64_t idx = (uint64_t)addr >> PAGESHIFT;
    if(properties_ptr->info_buffer[idx].inuse)
        return 1; //already in use
    properties_ptr->info_buffer[idx].inuse = 1;
    free_memory -= PAGESIZE;
    used_memory += PAGESIZE;
    return 0;
}

/* Allocate a page. Return 0 on success.*/
int alloc_pages(void* addr, size_t num_pages){
    for(size_t i = 0 ; i < num_pages; i++){
        if(alloc_page((void*)((uint64_t)addr + (i * PAGESIZE)))){
            free_pages(addr, i);
            return 1;
        }
    }
    return 0;
}

/* Free a page. Return 0 on success.*/
int free_page(void* addr){
    uint64_t idx = (uint64_t)addr >> PAGESHIFT;
    if(!properties_ptr->info_buffer[idx].inuse)
        return 1; //already freed
    properties_ptr->info_buffer[idx].inuse = 0;
    free_memory += PAGESIZE;
    used_memory -= PAGESIZE;
    return 0;
}

int free_pages(void* addr, size_t num_pages){
    for(size_t i = 0 ; i < num_pages; i++){
        //NOTE: (nick) worry about double free? probs not
        free_page((void*)((uint64_t)addr + (i * PAGESIZE)));
    }
    return 0;
}

/* Find the first free page and then allocate it*/
void* request_page(){
    for (uint64_t i = 0; i < properties_ptr->size; i++){
        if (properties_ptr->info_buffer[i].inuse || properties_ptr->info_buffer[i].reserved) continue;
        alloc_page((void *)(i * PAGESIZE));
        return (void *)(i * PAGESIZE);
    }
    return NULL;
}

void print_available_memory(){
    printf("Currently Used: %d\n", used_memory);
    printf("Currently Free: %d\n", free_memory);

}

void print_allocator(){
    uint64_t current_state = 0;
    uint64_t old_state = 0;
    uint64_t prev = 0;
    printf("Printing entire memory map...: %d \n", properties_ptr->size);
    print_available_memory();

    for (uint64_t i = 0; i < properties_ptr->size; i++){
        old_state = current_state;

        if (properties_ptr->info_buffer[i].inuse){ // inuse
            current_state = 1;
        }
        else if (properties_ptr->info_buffer[i].reserved){ //reserved
            current_state = 2;
        }
        else{ // free
            current_state = 3;
        }

        if ((current_state != old_state && old_state != 0) || (i + 1) == properties_ptr->size){ // change in state we printing
            switch(old_state){
                case 1:
                    printf("0x%llx - 0x%llx: in use\n", prev, (i - 1) * PAGESIZE);
                break;
                case 2:
                    printf("0x%llx - 0x%llx: reserved\n", prev, (i - 1) * PAGESIZE);
                    break;
                case 3:
                    printf("0x%llx - 0x%llx: free\n", prev, (i - 1) * PAGESIZE);
                default:
                    break;
            }
            prev = i * PAGESIZE;
        }
    }
}

