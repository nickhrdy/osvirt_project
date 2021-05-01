#include <mm.h>
/* private variables */
static char *mem_start_brk;  /* points to first byte of heap */
static char *mem_brk;        /* points to last byte of heap */
static char *mem_max_addr;   /* largest legal heap address */
static void *mmap_addr = (void *)0x58000000;


/*
 * mem_init - initialize the memory system model
 */
void memlib_init(void)
{
    //TODO call system call to map in stuff to mem_start_brk
    __syscall2(1, -1, -1);
    mem_max_addr = mem_start_brk + MAX_HEAP;  /* max legal heap address */
    mem_brk = mem_start_brk;                  /* heap is empty initially */
}

/*
 * mem_sbrk - simple model of the sbrk function. Extends the heap
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 */
void *mem_sbrk(int incr)
{
    char *old_brk = mem_brk;

    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	    __syscall1(0, (long)"mem_sbrk failed. probably out of memory\n");
	    return NULL;
    }
    mem_brk += incr;
    return (void *)old_brk;
}


int mm_init(void){
     return 1;
}

void * mm_malloc(size_t size){
    return NULL;
}

void * mm_realloc(void *addr, size_t size){
    mm_free(addr);
    return mm_malloc(size);
}

void mm_free(void *addr){

}


