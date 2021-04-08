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

#define DEBUG 0

void *generic_trap_hdl_ptr; /*generic exception handler; set in kernel_entry.S*/
void *pg_fault_hdl_ptr; /*page_fault exception handler; set in kernel_entry.S*/
void *timer_hdl_ptr; /*timer handler; set in kernel_entry.S*/
void *gp_hdl_ptr; /*general protection handler; set in kernel_entry.S*/
static gate_descriptor_t idt[256] __attribute__((aligned(16))); // IDT table
page_pte_t* faulting_page; //pointer to page we will purposefully fault on
uint64_t* replacement_page; // page to replace `faulting_page`

/* Fills out a vector of the IDT table */
static void x86_fillgate(int num, void *fn, int ist)
{
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
    set_pte(faulting_page, 0, (uint64_t)replacement_page >> 12, 1, 1);
    //Assume that the replacement page is the page after the user's pml
    write_cr3((uint64_t)(replacement_page - 512));
}  

/* Create a page table for the user */
uint64_t create_user_page_table(uint64_t* base, uint64_t* user_ptr, uint64_t size, uint64_t kernel_pdpe) {
int i;
    /*
    * user page table level 1
    * 1MB = 1024KB              1024KB * (1 page / 512KB) = 256 pages
    * first entry maps the stack, the second starts the user program and any
    * extra pages are padding
    */
    page_pte_t* p = (page_pte_t*) base;
    unsigned int num_pages = (size / 4096) + 1; //number of page that the code fits in

    set_pte(p, 0, (uint64_t)(user_ptr - 512) >> 12, 1, 1);
    for(i = 1; i <= num_pages; i++) { set_pte(p, i, (uint64_t)((user_ptr) + 512 * (i-1) ) >> 12, 1, 1); }
    for(i = num_pages+1; i < 512; i++) { set_pte(p, i, 0, 0, 1); }

    // user level 2
    // first entry point to the pte and the rest are blank
    page_pde_t* pde = (page_pde_t*) base + 512;
    set_pde(pde, 0, (uint64_t) p >> 12, 1, 1);
    for (i = 1; i < 512; i++) { set_pde(pde, i, 0, 0, 1); }

    // user level 3
    // last entry points to pde and the rest are blank
    page_pdpe_t* pdpe = (page_pdpe_t*) (base + 1024);
    for (i = 0; i < 511; i++) { set_pdpe(pdpe, i, 0, 0, 1); }
    set_pdpe(pdpe, 511, (uint64_t) pde >> 12, 1, 1);

    // user level 4
    // first entry points to kernel pdpe, last to user pdpe, rest are blank
    page_pml_t* pml = (page_pml_t*) (base + 1536);
    set_pml(pml, 0, kernel_pdpe >> 12, 1, 0); //offset to kernel pml known
    for (i = 1; i < 511; i++) { set_pml(pml, i, 0, 0, 1); }
    set_pml(pml, 511, (uint64_t) pdpe >> 12, 1, 1);


    //Assignment 2 Part 2: Set a replacement page in a global variable
    //and set faulting_page to the page that we'll try to break
    replacement_page = base + 2048;
    faulting_page = &p[256];

    return (uint64_t) pml;
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


/**
 *  kernel_ptr -> points region containing kernel code; also is the end of the kernel stack
 *  user_ptr -> points to region containing the user code; also is the end of the user stack
 *  page_table_ptr -> space for the page table (both kernel and user)
 *  tss_ptr -> points to region for tss; also is the end of the tss stack
 *  framebuffer -> reference to frame buffer
 *  size -> size of the user program in bytes
 */
void kernel_start(uint64_t* kernel_ptr, uint64_t* user_ptr, uint64_t* page_table_ptr, 
    uint64_t* tss_ptr, unsigned int* framebuffer, unsigned int size)
{
    syscall_init(); //initialize system calls
    fb_init(framebuffer, 800, 600);

    /* Create kernel page table */
    create_kernel_page_table(page_table_ptr);

    setup_interrupts((tss_segment_t*) tss_ptr);
    x86_lapic_enable(); //initialize local apic controller

    // Now create user page table. The user page table will be in the
    // same buffer as the kernel page table so the offset can be used to our advantage
    // point base to be after kernel pages
    uint64_t* base = page_table_ptr + (8413184 / 8);
    uint64_t kernel_pml = (uint64_t)(page_table_ptr + 1050624);
    uint64_t user_pml = create_user_page_table(base, user_ptr, size, kernel_pml);

    //Setup TLS for user
    setup_tls(base, tss_ptr + 1024);

    // find the address of the user stack and the user code. 
    // they share the address becuase of how they are allocated and mapped.
    uint64_t target_address = 0xFFFFFFFFC0001000;

    //switch page table and jump to user
    write_cr3((uint64_t) user_pml);
    user_stack = (void*)target_address;
    user_jump((void*)target_address);

    /* Never exit! */
	while (1) {};
}
