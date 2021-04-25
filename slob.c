#include <slob.h>

#define SMOL 256
#define BIG 1024
#define CHUNKY PAGESIZE

list_t slob_lists[3];
static size_t slob_sizes[3] = {256, 1024, PAGESIZE};

/* address ranges for each list. Used to naively for list membership when freeing */
static void* small_slob_bounds[2];
static void* medium_slob_bounds[2];
static void* large_slob_bounds[2];


static void __init_slob_list(void* base_addr, size_t num_pages, slob_type_t slob_type){
    list_t* list = &slob_lists[slob_type];
    uint64_t i;
    slob_blk_t* blk;
    uint64_t addr = (uint64_t) base_addr;

    list_init(&slob_lists[slob_type]);

    switch(slob_type){
        case SMALL:
            small_slob_bounds[0] = base_addr;
            small_slob_bounds[1] = base_addr + num_pages * PAGESIZE;
        break;
        case MEDIUM:
            medium_slob_bounds[0] = base_addr;
            medium_slob_bounds[1] = base_addr + num_pages * PAGESIZE;
        break;
        case LARGE:
            large_slob_bounds[0] = base_addr;
            large_slob_bounds[1] = base_addr + num_pages * PAGESIZE;
        break; 
        default:;
    }

    for(i = addr; i < addr + (num_pages * PAGESIZE); i += slob_sizes[slob_type]){
        //printf("%p\n", i);
        blk = (slob_blk_t*)i;
        blk->size = slob_sizes[slob_type];
        list_push_back(list, &blk->elem);
    }
    busy_loop();
}


void slob_init(size_t num_pages){
    if (num_pages < 3) HALT("Num pages given to slob allocator is less than 3, we got problems \n");
    size_t smol_pages = num_pages / 3;
    size_t big_pages = num_pages / 3;
    size_t chunky_pages = num_pages / 3;
    void* addr;
    // initalize free lists 
    //loop through each free list, adding all blocks to respective list
    if (! (addr = get_block(smol_pages))) HALT("slob_init: Failed to alloc small slob list");
    __init_slob_list(addr, smol_pages, SMALL);
    if (! (addr = get_block(big_pages))) HALT("slob_init: Failed to alloc medium slob list");
    __init_slob_list(addr, big_pages, MEDIUM);
    if (! (addr = get_block(chunky_pages))) HALT("slob_init: Failed to alloc large slob list");
    __init_slob_list(addr, chunky_pages, LARGE);
}


void *__slob_alloc(size_t size){
    //traverse list based on size range
    slob_type_t slob_size;
    if(size <= 256) slob_size = SMALL;
    else if (size <= 1024)slob_size = MEDIUM;
    else if(size <= PAGESIZE) slob_size = LARGE;
    else return NULL;

    list_elem_t* e;
    if(list_empty(&slob_lists[slob_size])){
        return NULL;
    }

    // first free block
    e = list_begin(&slob_lists[slob_size]);
    slob_blk_t* blk = list_entry(e, slob_blk_t, elem);
    list_remove(e);

    return (void*)blk;
}

static void __slob_free(void* addr){
    slob_type_t stype;
    slob_blk_t* blk = addr;

    if ( addr >= small_slob_bounds[0] && addr <= small_slob_bounds[1])
        stype = SMALL;
    if ( addr >= medium_slob_bounds[0] && addr <= medium_slob_bounds[1])
        stype = MEDIUM;
    if ( addr >= large_slob_bounds[0] && addr <= large_slob_bounds[1])
        stype = LARGE;

    blk->size = slob_sizes[stype];

    // insert at end of appropriate list
    list_insert(list_end(&slob_lists[stype]), &blk->elem);
}


//allocates memory through slab allocator.
void *kmalloc(size_t size){
    return __slob_alloc(size);
}

//allocates memory (and zeroes it out like calloc() in libc) through the slab allocator.
// void *kzalloc(size_t size){
//     void* addr = __slob_alloc(size);
//     __builtin_memset(addr, 0, size);
//     return addr;
// }

//resize existing allocation.
void * krealloc(void * addr, size_t size){
    kfree(addr);
    return kmalloc(size);
}

//frees memory previously allocated.
void kfree(void * addr){
    __slob_free(addr);
}

void kzfree(void * addr){
    //TODO ZERO OUT
    __slob_free(addr);
}

void slob_list_counts(){
    size_t i;
    for(i = 0; i < 3; i++){
        printf("Slob List #%d\n", i);
        printf("List size: %d\n", list_size(&slob_lists[i]));
    }
}

void debug_slob_lists(){
    size_t i;
    list_elem_t* e;
    slob_blk_t *blk;
    int counter = 0;
    printf("[?] Debugging slob list...\n");
    for(i = 0; i < 3; i++){
        counter = 0;
        if(!list_empty(&slob_lists[i])){
            printf("Slob List #%d\n", i);
            printf("List size: %d\n", list_size(&slob_lists[i]));
            busy_loop();
            for(e = list_begin(&slob_lists[i]); e != list_end(&slob_lists[i]); e = list_next(e)){
                blk = list_entry(e, slob_blk_t, elem);
                printf("slob_blk %d: %p -> size: %d\n", counter++, blk, blk->size);
            }
        }
    }
    printf("[?] End debugging slob list\n");
}
