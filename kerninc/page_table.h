#ifndef _PAGE_TABLE_H_
#define _PAGE_TABLE_H_

#include <types.h>

#define PT_INDEX_MASK 0x01FFULL  // 9 bits for parsing page table offsets

/* Struct definitions taken from:
 * https://www.amd.com/system/files/TechDocs/24593.pdf
 */

typedef struct page_pte {
    uint64_t present : 1;        // Bit P    (Present Bit)
    uint64_t writable : 1;       // Bit R/W  (Read/Write Bit)
    uint64_t usermode : 1;       // Bit U/S  (User/Supervisor Bit)
    uint64_t pwt : 1;            // Bit PWT  (Page-Level Writethrough)
    uint64_t pcd : 1;            // Bit PCD  (Page-Level Cache Disable)
    uint64_t accessed : 1;       // Bit A    (Accessed)
    uint64_t dirty : 1;          // Bit D    (Dirty)
    uint64_t pat : 1;            // Bit PAT  (Page-Attribute Table)
    uint64_t global : 1;         // Bit G    (Global)
    uint64_t avl : 3;            // Bit AVL  (Available to Software)
    uint64_t page_address : 40;  // Physical address (max)
    uint64_t avail : 7;          // Should be 0
    uint64_t pke : 4;            // Should be 0
    uint64_t nonexecute : 1;
} page_pte_t; /* Page table entry */

typedef struct page_pde {
    uint64_t present : 1;   // Bit P    (Present Bit)
    uint64_t writable : 1;  // Bit R/W  (Read/Write Bit)
    uint64_t usermode : 1;  // Bit U/S  (User/Supervisor Bit)
    uint64_t pwt : 1;       // Bit PWT  (Page-Level Writethrough)
    uint64_t pcd : 1;       // Bit PCD  (Page-Level Cache Disable)
    uint64_t accessed : 1;  // Bit A    (Accessed)
    uint64_t ign : 1;
    uint64_t o : 1;
    uint64_t ign2 : 1;
    uint64_t avl : 3;            // Bit AVL  (Available to Software)
    uint64_t page_address : 40;  // Physical address (max)
    uint64_t avail : 11;         // Should be 0
    uint64_t nonexecute : 1;
} page_pde_t; /* Page directory entry*/

typedef struct page_pdpe {
    uint64_t present : 1;   // Bit P    (Present Bit)
    uint64_t writable : 1;  // Bit R/W  (Read/Write Bit)
    uint64_t usermode : 1;  // Bit U/S  (User/Supervisor Bit)
    uint64_t pwt : 1;       // Bit PWT  (Page-Level Writethrough)
    uint64_t pcd : 1;       // Bit PCD  (Page-Level Cache Disable)
    uint64_t accessed : 1;  // Bit A    (Accessed)
    uint64_t ign : 1;
    uint64_t o : 1;
    uint64_t ign2 : 1;
    uint64_t avl : 3;            // Bit AVL  (Available to Software)
    uint64_t page_address : 40;  // Physical address (max)
    uint64_t avail : 11;         // Should be 0
    uint64_t nonexecute : 1;
} page_pdpe_t; /* PDPE directory entry */

typedef struct page_pml {
    uint64_t present : 1;   // Bit P    (Present Bit)
    uint64_t writable : 1;  // Bit R/W  (Read/Write Bit)
    uint64_t usermode : 1;  // Bit U/S  (User/Supervisor Bit)
    uint64_t pwt : 1;       // Bit PWT  (Page-Level Writethrough)
    uint64_t pcd : 1;       // Bit PCD  (Page-Level Cache Disable)
    uint64_t accessed : 1;  // Bit A    (Accessed)
    uint64_t ign : 1;
    uint64_t mbz : 2;
    uint64_t avl : 3;            // Bit AVL  (Available to Software)
    uint64_t page_address : 40;  // Physical address (max)
    uint64_t avail : 11;         // Should be 0
    uint64_t nonexecute : 1;
} page_pml_t; /* PML4E entry struct */

typedef struct cr3_info {
    uint64_t reserved : 3;
    uint64_t pwt : 1;
    uint64_t pcd : 1;
    uint64_t reserved2 : 7;
    uint64_t base_address : 40;
    uint64_t mbz : 12;
} cr3_info_t; /* CR3 register layout */

typedef struct page_table_indexer {
    uint64_t pml_idx;
    uint64_t pdpe_idx;
    uint64_t pde_idx;
    uint64_t pte_idx;
} page_table_indexer_t;

/* Writes the location of a PML4 entry into cr3 register */
static inline void write_cr3(unsigned long long cr3_value) {
    asm volatile("mov %0, %%cr3" : : "r"(cr3_value) : "memory");
}

/* Set the contents of a page table entry */
void set_pte(page_pte_t* pte_base, int n, uint64_t address, int present,
             int usermode);

/* Set the contents of a page directory entry */
void set_pde(page_pde_t* pde_base, int n, uint64_t address, int present,
             int usermode);

/* Set the contents of a pdpe entry*/
void set_pdpe(page_pdpe_t* pdpe_base, int n, uint64_t address, int present,
              int usermode);

/* Set the contents of a pml entry */
void set_pml(page_pml_t* pml_base, int n, uint64_t address, int present,
             int usermode);

/* Prints translations for all pages present in a page table*/
void debug_page_table(void* pml_ptr);

/* Get equivalent page table offsets for a virtual address */
void get_page_indexes(page_table_indexer_t* indexes, void* virtual_addr);

/* Maps a physical address to a virtual address in a page table */
int map_memory(page_pml_t* pml4, void* virtual_addr, void* physical_addr);

#endif