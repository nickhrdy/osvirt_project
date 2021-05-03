/*
 * boot.c - a kernel template (Assignment 1, ECE 6504)
 * Copyright 2021 Ruslan Nikolaev <rnikola@vt.edu>
 */
#include<fb.h>
#include<printf.h>
#include<kernel_syscall.h>
#include<interrupts.h>
#include<types.h>
#include<apic.h>
#include<msr.h>
#include<page_table.h>
#include<allocator.h>
#include<slob.h>
#include<halt.h>


static char* EFI_MEMORY_TYPE_STRINGS[] = {
    "EfiReservedMemoryType",
    "EfiLoaderCode",
    "EfiLoaderData",
    "EfiBootServicesCode",
    "EfiBootServicesData",
    "EfiRuntimeServicesCode",
    "EfiRuntimeServicesData",
    "EfiConventionalMemory",
    "EfiUnusableMemory",
    "EfiACPIReclaimMemory",
    "EfiACPIMemoryNVS",
    "EfiMemoryMappedIO",
    "EfiMemoryMappedIOPortSpace",
    "EfiPalCode"
};

char* memory_type_to_string(enum EFI_MEMORY_TYPES type){
    return EFI_MEMORY_TYPE_STRINGS[type];
}

#define DEBUG 0
#define TIME_START(a, b) b = *time_ptr; printf("%s Start Time: %d\n", a, b)
#define TIME_END(a, b) printf("%s Time elapsed: %d\n", a, *time_ptr - b)

void *generic_trap_hdl_ptr; /*generic exception handler; set in kernel_entry.S*/
void *pg_fault_hdl_ptr; /*page_fault exception handler; set in kernel_entry.S*/
void *timer_hdl_ptr; /*timer handler; set in kernel_entry.S*/
void *gp_hdl_ptr; /*general protection handler; set in kernel_entry.S*/
static gate_descriptor_t idt[256] __attribute__((aligned(16))); // IDT table
page_pte_t* faulting_page; //pointer to page we will purposefully fault on
uint64_t* replacement_page; // page to replace `faulting_page`
page_pml_t* user_pml;
extern uint64_t* time_ptr;

/* Fills out a vector of the IDT table */
static void x86_fillgate(int num, void *fn, int ist){
    gate_descriptor_t* gd = &(idt[num]);

    gd->gd_hioffset = (uint64_t)fn >> 16;
    gd->gd_looffset = (uint64_t)fn & 0xFFFF;

    gd->gd_selector = 0x8;
    gd->gd_ist = ist;
    gd->gd_type = 14;
    gd->gd_dpl = 0;
    gd->gd_p = 1;

    gd->gd_zero = 0;
    gd->gd_xx1 = 0;
    gd->gd_xx2 = 0;
    gd->gd_xx3 = 0;
}

/* Fills the IDT table */
static void create_idt_table(){
    int i;
    // fill with generic handler
    for(i = 0; i < 32; i++){
        x86_fillgate(i, generic_trap_hdl_ptr, 0);
    }

    x86_fillgate(13, gp_hdl_ptr, 0);
    x86_fillgate(14, pg_fault_hdl_ptr, 0);
    x86_fillgate(48, timer_hdl_ptr, 0);
}

/* generic fault handler */
void x86_trap_generic(uint64_t rsp_addr){
    printf("\n[=] GENERIC EXCEPTION (%%rsp == 0x%llx)\n", rsp_addr);
}

/* general protection fault handler */
void x86_trap_13(){
    printf("\n[-] GENERAL PROTECTION FAULT\n");
}

/* page fault handler */
void x86_trap_14(uint64_t rsp_addr){
    printf("\n[-] PAGE_FAULT (%%rsp == 0x%llx)\n", rsp_addr);
    halt();
    //set_pte(faulting_page, 0, (uint64_t)replacement_page >> 12, 1, 1);
    //Assume that the replacement page is the page after the user's pml
    //write_cr3((uint64_t)(replacement_page - 512));
}

void setup_interrupts(tss_segment_t* tss_ptr){
    // Create IDT
    create_idt_table();
    idt_pointer_t idt_ptr;
    idt_ptr.base = (uint64_t)(void*)&idt;
    idt_ptr.limit = sizeof(idt) - 1;
    load_idt(&idt_ptr);

    // Create TSS segment
    tss_segment_t* tss = tss_ptr;
    __builtin_memset(tss, 0, sizeof(tss_segment_t));
    tss->rsp[0] = (uint64_t) tss_ptr;
    tss->iopb_base = sizeof(tss_segment_t);
    load_tss_segment(0x28, tss);
}

void setup_tls(uint64_t* user_page_table_base, uint64_t* base){
    set_pte((page_pte_t*)user_page_table_base, 257, (uint64_t)(base) >> 12, 1, 1);
    tls_block_t* tls_block = (tls_block_t*)base;
    tls_block->myself = (tls_block_t*)0xFFFFFFFFC0101000;
    __builtin_memset(tls_block->padding, 0, 4096-8);
    wrmsr(MSR_FS, (uint64_t)0xFFFFFFFFC0101000);
}

void print_memory_map_contents(boot_info_t* b_info){
    uint64_t num_map_entries = b_info->memory_map_size / b_info->memory_map_desc_size;
    for (int i = 0 ; i < num_map_entries; i++){
        efi_memory_descriptor_t* desc = (efi_memory_descriptor_t*)((uint64_t)b_info->memory_map + (i * b_info->memory_map_desc_size));
        printf("Entry %d, Size %llu pages, Type %d\n", i, desc->num_pages, desc->type);
    }
}

void eval_buddy_system(void){
    uint64_t t;
    TIME_START("Correctness", t);
    //lots of grabs of diff size
    for(uint64_t i = 2 ; i < 1 << 5; i <<= 1 ){
        void *a = get_block(i);
        void *b = get_block(i);
        printf("%p %p\n", a, b);
        free_block(a);
        free_block(b);
    }
    TIME_END("Correctness", t);

    debug_buddy_lists();

    TIME_START("Full Allocate", t);
    for(uint64_t i = 1 << 5 ; i > 2 ; i >>= 1 ){
        get_block(i);
        get_block(i);
    }
    TIME_END("Full Allocate", t);

    debug_buddy_lists();
}

void eval_slob_allocator(void){
    size_t list_size = 10000;
    slob_init(list_size * 3);
    uint64_t t;

    TIME_START("Correctness", t);
    for(size_t i = 0; i < list_size; i++){
        void * a = kmalloc(256);
        void * b = kmalloc(256);

        krealloc(b, 4096);
        krealloc(a, 1024);
        krealloc(b, 1024);
        krealloc(a, 4096);

        kfree(b);
        kfree(a);
    }
    TIME_END("Correctness", t);

    debug_slob_lists();

    TIME_START("Full Allocate", t);
    for(size_t i = 0; i < list_size; i++){
        kmalloc(256);
    }
    for(size_t i = 0; i < list_size; i++){
        kmalloc(1024);
    }
    for(size_t i = 0; i < list_size; i++){
        kmalloc(4096);
    }
    TIME_END("Full Allocate", t);

    debug_slob_lists();
}



void kernel_start(uint64_t* kernel_ptr, boot_info_t* b_info) {
    int rc;
    syscall_init(); //initialize system calls
    fb_init(b_info->framebuffer, 1600, 900);
    init_page_properties(b_info->memory_map, b_info->memory_map_size, b_info->memory_map_desc_size);

    printf("[|] Kernel code size: %d b // %d pg\n", b_info->kernel_code_size, b_info->kernel_code_size / PAGESIZE + 1);
    rc = alloc_pages(kernel_ptr, b_info->kernel_code_size / PAGESIZE + 1);
    printf("[|] Alloc_pages returned %d\n", rc);
    alloc_page( (void*)((uint64_t)(b_info->framebuffer) & ~PAGESHIFT));
    printf("[|] Largest segment size: %d\n", get_largest_segment_size(b_info->memory_map, b_info->memory_map_size, b_info->memory_map_desc_size) / 1024);

    /* Create kernel page table */
    print_allocator();
    debug_buddy_lists();
    page_pml_t* kernel_pml = (page_pml_t*) get_block(1);
    clear_page(kernel_pml);
    uint64_t size = get_memory_map_size(b_info->memory_map, b_info->memory_map_size, b_info->memory_map_desc_size);
    //debug_buddy_lists();

    printf("Kernel pml: %p\n", kernel_pml);
    for(uint64_t i = 0; i < size; i += PAGESIZE){
        if((rc = map_memory(kernel_pml, (void*)i, (void*)i, 0))){
            printf("[!] Failed to map kernel table! Status(%d)\n", rc);
            halt();
        }
    }

    /* Map framebuffer into memory */
    for(uint64_t i  = (uint64_t)b_info->framebuffer; i < (uint64_t)b_info->framebuffer + (1 << 24); i += PAGESIZE){
        if((rc = map_memory(kernel_pml, (void*)i, (void*)i, 0))){
            printf("[!] Failed to map framebuffer! Status(%d)\n", rc);
            halt();
        }
    }

    // map last gig into kernel since interrupt addresses are there
    for(uint64_t i  = 0xc0000000; i < 0x100000000; i += PAGESIZE){
        if((rc = map_memory(kernel_pml, (void*)i, (void*)i, 0))){
            printf("[!] Failed to map framebuffer! Status(%d)\n", rc);
            halt();
        }
    }

    x86_lapic_enable(); //initialize local apic controller
    setup_interrupts((tss_segment_t*) b_info->tss_buffer);

    // run our benchmarks
    //eval_buddy_system();
    //eval_slob_allocator();


    //setup user stuff
    user_pml = (page_pml_t*) get_block(1);
    clear_page(user_pml);
    user_pml[0].page_address = kernel_pml[0].page_address;
    user_pml[0].writable = 1;
    user_pml[0].present = 1;
    user_pml[0].usermode = 0;

    printf("User pml: %p\n", user_pml);

    for(uint64_t i = 0 ; i < b_info->user_code_size; i += PAGESIZE){
        if((rc = map_memory(user_pml, (void*)(0x8000003000 + i), (void*)( (uint64_t)b_info->user_buffer + i), 1))){
            printf("[!] Failed to map user table! Status(%d)\n", rc);
            halt();
        }
    }


    void* user_stack_ptr = get_block(1);
    if((rc = map_memory(user_pml, (void*)(0x8000001000), user_stack_ptr, 1))){
        printf("[!] Failed to map framebuffer! Status(%d)\n", rc);
        halt();
    }

    user_stack = (void*) 0x8000002000;


    printf("[|] Overwriting cr3\n");
    write_cr3((uint64_t)user_pml);
    user_jump((void*)(0x8000003000));


    HALT("[|] We made it to end of kernel!\n");
}
