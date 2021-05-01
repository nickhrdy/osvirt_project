#include <kernel_syscall.h>
#include <types.h>
#include <msr.h>
#include <printf.h>
#include <allocator.h>
#include <page_table.h>

void *kernel_stack; /* Initialized in kernel_entry.S */
void *user_stack = NULL; /* TODO: Must be initialized to a user stack region */
void *syscall_entry_ptr; /* Points to syscall_entry(), initialized in kernel_entry.S; use that rather than syscall_entry() when obtaining its address */
extern page_pml_t* user_pml;
void* t;


typedef struct boundary_block{
    size_t free:1;
    size_t size:63;
}boundary_block_t;


void debug_heap(){
    printf("[?] Debugging user heap...\n");
    boundary_block_t* tmp;

    char* ptr = (char*) t;
    ptr += sizeof(boundary_block_t); //move past first fence

    tmp = (boundary_block_t*)ptr;
    printf("Heap (not including first fence) starts at %p\n", tmp);
    if (!tmp->free && !tmp->size){
        printf("Heap empty\n");
        return;
    }

    while(tmp->size){ //print and hop until last fence
        printf("%p, size: %d, free: %d\n", tmp, tmp->size, tmp->free);
        ptr += tmp->size + sizeof(boundary_block_t) * 2;
        tmp = (boundary_block_t*)ptr;
    }
    printf("[?] Done debugging user heap...\n");
}

long assign_heap(long size){
    uint64_t base_vaddr = 0x18000000000;
    if(!(t = get_block(size)) ){
        printf(">:( :: (%d)\n", t);
        return 1;
    }

    for(int i = 0; i < size; i += PAGESIZE)
        map_memory(user_pml, (void*)(base_vaddr + i), t + (PAGESIZE / 8) * i, 1);
    //mem stuff
    printf("kernel perspective heap start %p\n", t);
    printf("[+] SYSCALL: Mapped in %d pages starting at %p\n", size, (void*)base_vaddr);
    return base_vaddr;
}

long do_syscall_entry(long n, long a1, long a2, long a3, long a4, long a5)
{
    // TODO: a system call handler
    // the system call number is in 'n'
    // make sure it is valid

    //define read as 0
    if( n < 0 || n > 3)
        return -1; // unknown syscall
    else if (n == 0)
        printf((char*)a1);
    else if (n==1)
        return assign_heap(a1);
    else if (n==2) //check heap
        debug_heap();
    else if (n==3)
        printf((char*)a1, a2);
    return 0; /* Success */
}

void syscall_init(void)
{
    /* Enable SYSCALL/SYSRET */
    wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 0x1);

    /* GDT descriptors for SYSCALL/SYSRET (USER descriptors are implicit) */
    wrmsr(MSR_STAR, ((uint64_t) GDT_KERNEL_DATA << 48) | ((uint64_t) GDT_KERNEL_CODE << 32));

    // TODO: register a system call entry point
    wrmsr(MSR_LSTAR, (uint64_t) syscall_entry_ptr);

    /* Disable interrupts (IF) while in a syscall */
    wrmsr(MSR_SFMASK, 1U << 9);
}
