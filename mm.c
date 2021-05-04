#include <mm.h>
#define PAGESIZE 4096
#define TAGSIZE sizeof(boundary_block_t)
#define PRINT(msg) __syscall1(0, (long)msg)
#define PRINTF(msg, a) __syscall2(3, (long)msg, a)
/* private variables */
static char *mem_start_brk = NULL;  /* points to first byte of heap */
static char *mem_brk = NULL;        /* points to last byte of heap */
static char *mem_max_addr = NULL;   /* largest legal heap address */

/*
 *    Extends the heap by incr bytes and returns the start address of the new 
 *    area. The heap cannot be shrunk.
 */
void *mem_sbrk(int incr)
{
    char *old_brk = mem_brk;

    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	    __syscall1(0, (long)"[!] mem_sbrk() failed. Probably out of memory!!\n");
	    return NULL;
    }
    mem_brk += incr;
    return (void *)old_brk;
}

/* |header||payload| */
static void* get_payload(boundary_block_t* cur_block){
    return (void*)(((char*)cur_block) + TAGSIZE);
}

/* |header||payload||footer||next_block_header|.... */
static boundary_block_t* get_next(boundary_block_t* cur_block){
    if((void*)cur_block == mem_brk - TAGSIZE) // tail case
        return NULL;
    if((void*)cur_block == mem_start_brk) return cur_block + 1; // head case
    return (boundary_block_t*)(((char*)cur_block) + cur_block->size + TAGSIZE*2);
}

/* |prev_block_header||prev_block_payload||prev_block_footer||header|.... */
static boundary_block_t* get_prev(boundary_block_t* cur_block){
    if((void*)cur_block == mem_start_brk) // head case
        return NULL;

    boundary_block_t* prev_footer = (boundary_block_t*)(((char*)cur_block) - TAGSIZE);
    return (boundary_block_t*)(((char*)cur_block) - TAGSIZE*2 - prev_footer->size);
}

/* |header||payload||footer| */
static boundary_block_t* get_footer(boundary_block_t* cur_block){
    return (boundary_block_t*)(((char*)cur_block) + TAGSIZE + cur_block->size);
}

static boundary_block_t* find_block(size_t size){
    boundary_block_t* cur_block;
    for(cur_block = (boundary_block_t*)mem_start_brk; cur_block != NULL; cur_block = get_next(cur_block)){
        // __syscall2(3, (long)"[|] Found Cur Block: %p\n", (long)cur_block);
        // __syscall2(3, (long)"[|] Cur Block size: %llu\n", (long)cur_block->size);
        // __syscall2(3, (long)"[|] Cur Block free: %llu\n", (long)cur_block->free);
        if (cur_block->free && cur_block->size >= size) return cur_block;
    }
    return NULL;
}

/*mark header and footer as used*/
static void mark_used(boundary_block_t* cur_block, size_t size){
    cur_block->free = 0;
    cur_block->size = size;
    get_footer(cur_block)->free = 0;
    get_footer(cur_block)->size = size;
}

/*mark header and footer as free*/
static void mark_free(boundary_block_t* cur_block, size_t size){
    cur_block->free = 1;
    cur_block->size = size;
    get_footer(cur_block)->free = 1;
    get_footer(cur_block)->size = size;
}

/*call mem_sbrk to make the heap larger*/
boundary_block_t* extend_heap(size_t size){
    size_t to_extend = max(size + 2*TAGSIZE, MIN_BLOCK_SIZE);
    boundary_block_t* new_last_block = (boundary_block_t*)((char*)mem_sbrk(to_extend) - TAGSIZE);
    //__syscall2(3, (long)"[?] extend heap: new last block => %p\n", (long)(new_last_block));
    mark_free(new_last_block, size);
    get_next(new_last_block)->free = 0;
    get_next(new_last_block)->size = 0;
    return new_last_block;
}


//whats left after split is bigger than min block size
// whats left = old_size - to_use - 2*TAGSIZE
// if whats left >= minblocksize > split
static void split(boundary_block_t* cur_block, int to_use){
    int old_size = (int)cur_block->size;
    int split_size = max(0,((int)old_size) - to_use - ((int)TAGSIZE) - ((int)TAGSIZE));

    if (split_size > MIN_BLOCK_SIZE){
        mark_used(cur_block, to_use);
        mark_free(get_next(cur_block), split_size);
    }
    else{
        mark_used(cur_block, old_size);
    }
}


/*mark header and footer as free*/
static void coalesce(boundary_block_t* cur_block){
    int prev_free, prev_size, next_free, next_size;

    if( (cur_block - 1)->size == 0){ // first fence check
        prev_free = 0;
        prev_size = 0;
    }
    else{
        prev_free = get_prev(cur_block)->free;
        prev_size = get_prev(cur_block)->size;
    }
    next_free = get_next(cur_block)->free;
    next_size = get_next(cur_block)->size;

    if(prev_free && next_free)
        /*|prev_head|prev_payload|prev_footer||cur_head|cur_payload|cur_footer||nxt_head|nxt_payload|nxt_footer|*/
        /*|head|payload|foot|*/
        mark_free(get_prev(cur_block), prev_size + next_size + cur_block->size + TAGSIZE * 4);
    else if(prev_free)
        /*|prev_head|prev_payload|prev_footer||cur_head|cur_payload|cur_footer|*/
        /*|head|payload|foot|*/
        mark_free(get_prev(cur_block), prev_size + cur_block->size + TAGSIZE * 2);
    else if(next_free)
        /*|cur_head|cur_payload|cur_footer||nxt_head|nxt_payload|nxt_footer|*/
        /*|head|payload|foot|*/
        mark_free(cur_block, next_size + cur_block->size + TAGSIZE * 2);
    else
        mark_free(cur_block, cur_block->size);
}

/*
 * mem_init - initialize the memory system model
 */
void memlib_init(void)
{
    __syscall1(0, (long)"[?] In memlib_init()\n");
    mem_start_brk = (char*) __syscall1(1, MAX_HEAP / PAGESIZE + 1); /*request memory up front*/
    mem_max_addr = mem_start_brk + MAX_HEAP;  /* max legal heap address */
    mem_brk = mem_start_brk;                  /* heap is empty initially */

    //initial sbrk call and stick fences in there
    boundary_block_t* initial = (boundary_block_t* )mem_sbrk(MIN_BLOCK_SIZE);
    // __syscall2(3, (long)"initial return from sbrk: %p\n", (long)initial);
    // __syscall2(3, (long)"tail location: %p\n", (long)(initial + 1));
    initial[0].size = 0;
    initial[0].free = 0;
    initial[1].size = 0;
    initial[1].free = 0;
}

void * mm_malloc(size_t size){
    //__syscall2(3, (long)"[|] Mallocing for size: 0x%llx!\n", (long)size);
    if(!mem_max_addr){
        __syscall1(0, (long)"[?] mm_malloc: calling memlib_init!\n");
        memlib_init();
    }

    if (!size)
        return NULL;

    size = max(MIN_BLOCK_SIZE, size);
    boundary_block_t *cur_block = find_block(size);

    if (cur_block == NULL){
        //no fit found grow the heap
        //__syscall1(0, (long)"[|] Extending heap!\n");
        cur_block = extend_heap(size);
        if (cur_block == NULL){
            __syscall1(0, (long)"[!] Failed to extend heap!!\n");
            return NULL;
        }
    }
    split(cur_block, size);
    return get_payload(cur_block);
}

void * mm_realloc(void *addr, size_t size){
    if(!mem_max_addr)
        memlib_init();

    if (!size)
        return NULL;

    mm_free(addr);
    return mm_malloc(size);
}

void mm_free(void *addr){
    if(!addr) return;
    boundary_block_t *cur_block = (boundary_block_t *)(((char*)addr) - TAGSIZE);
    coalesce(cur_block);
}

/* Debug the heap from user_space */
void debug_heap_user(){
    PRINT("[?] Debugging user heap...\n");
    boundary_block_t* tmp;

    char* ptr = (char*) mem_start_brk;
    ptr += TAGSIZE; //move past first fence

    tmp = (boundary_block_t*)ptr;
    PRINTF("Heap (not including first fence) starts at %p\n", (uint64_t)tmp);
    
    if (!tmp->free && !tmp->size){
        PRINT("Heap empty\n");
        return;
    }

    while(tmp->size){ //print and hop until last fence
        PRINTF("%p, ", (uint64_t)tmp);
        PRINTF("size: %d, ", tmp->size);
        PRINTF("%d\n", tmp->free);
        ptr += (tmp->size + TAGSIZE * 2);
        tmp = (boundary_block_t*)ptr;
    }
    PRINT("[?] Done debugging user heap...\n");
}