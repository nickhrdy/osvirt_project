#pragma once

#include <types.h>

/* GDT, see kernel_entry.S */
extern uint64_t gdt[];

/*
 * NOTE: When declaring the IDT table, make
 * sure it is properly aligned, e.g.,
 *
 * ... idt[256] __attribute__((aligned(16)));
 * rather than just idt[256]
 *
 * The TSS segment _must_ be page-aligned, so you
 * can even allocate a dedicated page for it.
 * Please note that if TSS is not page-aligned, you
 * may encounter very weird issues that are hard
 * to track or debug!
 */

/* Load IDT */

/* Please define and initialize this variable and then load
   this 80-bit "pointer" (it is similar to GDT's 80-bit "pointer") */
struct idt_pointer {
	uint16_t limit;
	uint64_t base;
} __attribute__((__packed__));

typedef struct idt_pointer idt_pointer_t;

static inline void load_idt(idt_pointer_t *idtp)
{
	__asm__ __volatile__ ("lidt %0; sti" /* also enable interrupts */
		:
		: "m" (*idtp)
	);
}

/*
 * TSS segment, see also https://wiki.osdev.org/Task_State_Segment
 */
struct tss_segment {
	uint32_t reserved1;
	uint64_t rsp[3]; /* rsp[0] is used, everything else not used */
	uint64_t reserved2;
	uint64_t ist[7]; /* not used, initialize to 0s */
	uint64_t reserved3;
	uint16_t reserved4;
	uint16_t iopb_base; /* must be sizeof(struct tss), I/O bitmap not used */
} __attribute__((__packed__));

typedef struct tss_segment tss_segment_t;

/*
 * This function initializes the TSS descriptor in GDT, you have
 * to place two 64-bit dummy 0x0 entries in GDT and specify
 * the selector of the first entry here.
 *
 * After initializing the GDT entries, this function will load the
 * TSS descriptor from GDT as the final step.
 *
 * You have to specify GDT's selector, the base address and _limit_
 * for the TSS segment.
 * 
 * selector -> offset; tss -> page 
 */
static inline
void load_tss_segment(uint16_t selector, tss_segment_t *tss)
{
	uint64_t base = (uint64_t) tss;
	uint16_t limit = sizeof(tss_segment_t) - 1;

	/* The TSS descriptor is initialized according to AMD64's Figure 4-22,
	   https://www.amd.com/system/files/TechDocs/24593.pdf */

	/* Initialize GDT's dummy entries */
	uint64_t *tss_gdt = (void *) gdt + selector;
	/* Present=1, DPL=0, Type=9 (TSS), 16-bit limit, lower 32 bits of 'base' */
	tss_gdt[0] = ((base & 0xFF000000ULL) << 32) | (1ULL << 47) | (0x9ULL << 40) | ((base & 0x00FFFFFFULL) << 16) | limit;
	/* Upper 32 bits of 'base' */
	tss_gdt[1] = base >> 32;

	/* Load TSS, use the specified selector */
	__asm__ __volatile__ ("ltr %0"
		:
		: "rm" (selector)
	);
}
