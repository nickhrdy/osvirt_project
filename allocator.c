#include <types.h>
#include <allocator.h>
#include <printf.h>
#include <halt.h>

#define NUM_BUDDY_LISTS 32

static int __reserve_page(void*addr);
static int __reserve_pages(void* addr, size_t num_pages);
static int __unreserve_page(void*addr);
static int __unreserve_pages(void* addr, size_t num_pages);

/* information about all page frames */
static page_properties_t properties_ptr = {0, NULL};
static buddy_block_t* buddy_pool;
static int free_memory = 0;
static int used_memory = 0;
static size_t num_buddy_pages;
static uint64_t offset;

static list_t buddy_lists[NUM_BUDDY_LISTS];

/**
 * ####################
 *  BUDDY FUNCTIONS
 * ####################
 */


/* Match number of pages to list index */
static size_t pages_to_buddy_index(size_t num_pages){
    int i = 0;
    while(num_pages >> ++i);
    return i - 1;
}

/* Find the first free buddy then allocate him*/
static buddy_block_t* __request_buddy(){
    uint64_t i;

    for (i = 0; i < num_buddy_pages * (PAGESIZE / sizeof(buddy_block_t)); i++){
        if (buddy_pool[i].physical_addr != 0) continue; //skip if in use
        return (buddy_block_t*) (buddy_pool + i);
    }
    return NULL;
}

/* Find a buddy assigned to the corresponding physical address */
static buddy_block_t* __find_buddy(void* physical_address){
    uint64_t i;
    uint64_t addr = (uint64_t)physical_address;

    for (i = 0; i < num_buddy_pages * (PAGESIZE / sizeof(buddy_block_t)); i++){
        if (buddy_pool[i].physical_addr == addr)
            return (buddy_block_t*) (buddy_pool + i);
    }
    return NULL;
}

/* Initialize buddy system with segment of memory */
static int init_buddy(void* addr, size_t total_mem_num_pages){
    uint64_t i;
    size_t list_index;
    buddy_block_t* blk;
    size_t remaining_pages = total_mem_num_pages;
    offset = (uint64_t)addr;

    // init all lists
    for (i = 0; i < NUM_BUDDY_LISTS; i++) list_init(&buddy_lists[i]);

    // place the inital memory block in the corresponding list
    list_index = pages_to_buddy_index(remaining_pages);
    if (remaining_pages < 1 << list_index){
        // account for overshoot
        list_index--;
    }

    // add big chunk to list
    if(!(blk = __request_buddy()))
        HALT("[!] Failed to initialize buddy!\n");

    // set attributes
    blk->physical_addr = (uint64_t)addr;
    blk->size = 1 << list_index;

    // insert
    list_insert(list_tail(&buddy_lists[list_index]), &blk->elem);

    // update remaining pages and move address to match
    remaining_pages -= 1 << list_index;
    addr += (1 << list_index) / 8 * PAGESIZE;


    return 0;
}

/* Add a buddy block to the correct list */
void __add_block(buddy_block_t* blk){
    size_t list_index = pages_to_buddy_index(blk->size);
    list_push_back(&buddy_lists[list_index], &blk->elem);
}

/* Remove a buddy block from it's list */
void __remove_block(buddy_block_t* blk){
    list_remove(&blk->elem);
}

/* Get a memory block from the free blocks */
void* get_block(size_t num_pages){
    size_t i, j;
    list_elem_t* elem;
    buddy_block_t *blk, *buddy;

    // find list index
    size_t list_index = pages_to_buddy_index(num_pages);

    // loop through lists trying to find a suitable block
    for(i = list_index; i < NUM_BUDDY_LISTS; i++){

        // ...if a block is available...
        if(!list_empty(&buddy_lists[i])){
            // grab pointer to block
            elem = list_front(&buddy_lists[i]);
            blk = list_entry(elem, buddy_block_t, elem);

            // split block until reaching an appropriate size
            for(j = i-1; j > list_index && j > 0; j--){ // <-- Not sure if condition should be > or >=
                // try to request a new buddy struct before splitting
                if(!(buddy = __request_buddy()))
                    HALT("[!] Failed to get buddy struct from pool!\n");

                // split area
                buddy->physical_addr = blk->physical_addr + (blk->size * PAGESIZE / 2);
                buddy->size = blk->size / 2;
                blk->size = blk->size / 2;

                //add new bud to list
                __add_block(buddy);
            }

            // remove block from list and return address
            __remove_block(blk);
            return (void*)blk->physical_addr;
        }
    }
    return NULL; // didn't find suitable block
}

/* free an assigned block */
void free_block(void* addr){
    buddy_block_t *blk = NULL, *buddy = NULL;
    uint64_t pair_addr;
    list_elem_t* e;
    // naive search for buddy that was mapped to this address
    if( !(blk = __find_buddy(addr)))
        HALT("[!] Failed to get buddy struct from pool!\n");

    /* try to find block's buddy */
    size_t list_index = pages_to_buddy_index(blk->size);
    pair_addr = ((((uint64_t) addr - offset) ^ (1 << (list_index + PAGESHIFT))) + offset);

    // search list for buddy
    for(e  = list_begin(&buddy_lists[list_index]); e != list_end(&buddy_lists[list_index]); e = list_next(e)){
        buddy = list_entry(e, buddy_block_t, elem);
        if(buddy->physical_addr == pair_addr){
            break;
        }
    }
    if(e == list_end(&buddy_lists[list_index])){
        __add_block(blk);
    } else {
        __remove_block(buddy);
        if(blk->physical_addr > buddy->physical_addr) {
            buddy->size *= 2;
            free_block((void*)buddy->physical_addr);
        }
        else {
            blk->size *= 2;
            free_block((void*)blk->physical_addr);
        }
    }
}

/**
 * ####################
 *  END BUDDY FUNCTIONS
 * ####################
 */

/* Unreserve a page. Return 0 on success.*/
static int __unreserve_page(void* addr){
    uint64_t idx = (uint64_t)addr >> PAGESHIFT;
    if(!properties_ptr.info_buffer[idx].reserved)
        return 1; //already unreserved
    properties_ptr.info_buffer[idx].reserved = 0;
    free_memory += PAGESIZE;
    used_memory -= PAGESIZE;
    return 0;
}

/* Unreserve pages. Return 0 on success.*/
static int __unreserve_pages(void* addr, size_t num_pages){
    for(size_t i = 0 ; i < num_pages; i++){
        __unreserve_page((void*)((uint64_t)addr + (i * PAGESIZE)));
    }
    return 0;
}

/* Reserve a page. Return 0 on success.*/
static int __reserve_page(void* addr){
    uint64_t idx = (uint64_t)addr >> PAGESHIFT;
    if(properties_ptr.info_buffer[idx].reserved)
        return 1; //already reserved
    properties_ptr.info_buffer[idx].reserved = 1;
    free_memory -= PAGESIZE;
    used_memory += PAGESIZE;
    return 0;
}

/* Reserve pages. Return 0 on success.*/
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

uint64_t get_largest_segment_size(efi_memory_descriptor_t* memory_map, uint64_t memory_map_size, uint64_t memory_map_desc_size){
    uint64_t largest = 0;

    for (uint64_t i = 0; i < memory_map_size / memory_map_desc_size; i++){
        efi_memory_descriptor_t* desc = (efi_memory_descriptor_t*)((uint64_t)memory_map + (i * memory_map_desc_size));
        if (desc->type == EFI_CONVENTIONAL_MEMORY && desc->num_pages * PAGESIZE > largest){
            largest = desc->num_pages * PAGESIZE;
        }
    }
    return largest;
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
    uint64_t num_total_mem_pages = get_memory_map_size(memory_map, memory_map_size, memory_map_desc_size) / PAGESIZE;
    properties_ptr.size = num_total_mem_pages;
    properties_ptr.info_buffer = (page_info_t*)addr;
    __builtin_memset(properties_ptr.info_buffer, 0, sizeof(page_info_t) * bitmap_size);

    //set sizes
    free_memory = get_memory_map_size(memory_map, memory_map_size, memory_map_desc_size);
    used_memory = 0;

    //reserve special segments that should not ever be used to kernel / user
    __reserve_page(0);
    reserve_special_segments(memory_map, memory_map_size, memory_map_desc_size);

    size_t num_bitmap_pages = properties_ptr.size * 8 / PAGESIZE + 1;
    num_buddy_pages = largest / PAGESIZE * sizeof(buddy_block_t) / PAGESIZE + 1;

    buddy_pool = (buddy_block_t*)(addr + (PAGESIZE * num_bitmap_pages));
    __reserve_pages(addr, num_bitmap_pages); //reserve bitmap
    __reserve_pages(buddy_pool, num_buddy_pages); //reserve buddypool
    for(int i = 0; i < num_buddy_pages; i++){
        __builtin_memset((void*)buddy_pool + (PAGESIZE / 8 * i), 0, PAGESIZE);
    }

    //init buddy
    init_buddy((void*)((uint64_t)buddy_pool + PAGESIZE * num_buddy_pages), (largest / PAGESIZE) - num_bitmap_pages - num_buddy_pages);
    return 0;
}

/* Allocate a page. Return 0 on success.*/
int alloc_page(void* addr){
    uint64_t idx = (uint64_t)addr >> PAGESHIFT;
    if(properties_ptr.info_buffer[idx].inuse)
        return 1; //already in use
    properties_ptr.info_buffer[idx].inuse = 1;
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
    if(!properties_ptr.info_buffer[idx].inuse)
        return 1; //already freed
    properties_ptr.info_buffer[idx].inuse = 0;
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
    for (uint64_t i = 0; i < properties_ptr.size; i++){
        if (properties_ptr.info_buffer[i].inuse || properties_ptr.info_buffer[i].reserved) continue;
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
    printf("Printing entire memory map...: %d \n", properties_ptr.size);
    print_available_memory();

    for (uint64_t i = 0; i < properties_ptr.size; i++){
        old_state = current_state;

        if (properties_ptr.info_buffer[i].inuse){ // inuse
            current_state = 1;
        }
        else if (properties_ptr.info_buffer[i].reserved){ //reserved
            current_state = 2;
        }
        else{ // free
            current_state = 3;
        }

        if ((current_state != old_state && old_state != 0) || (i + 1) == properties_ptr.size){ // change in state we printing
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

void debug_buddy_lists(){
    size_t i;
    list_elem_t* e;
    buddy_block_t *blk;

    printf("[?] Debugging buddy list...\n");
    for(i = 0; i < NUM_BUDDY_LISTS; i++){
        if(!list_empty(&buddy_lists[i])){
            printf("\tBuddy List #%d (size = %d)\n", i, 1 << i);
            for(e = list_begin(&buddy_lists[i]); e != list_end(&buddy_lists[i]); e = list_next(e)){
                blk = list_entry(e, buddy_block_t, elem);
                printf("   buddy: %p -> size: 0x%llx, paddr: %p\n", blk, blk->size * PAGESIZE, (void*)blk->physical_addr);
            }
        }
    }
    printf("[?] End debugging buddy list\n");
}

