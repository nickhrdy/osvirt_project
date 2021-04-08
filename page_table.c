#include<page_table.h>
#include<printf.h>

void set_pte(page_pte_t* pte_base, int n, uint64_t address, int present, int usermode){
    page_pte_t* pte = &(pte_base[n]);
    pte->present = present; 
    pte->writable = 1;
    pte->usermode = usermode; 
    pte->pwt = 0; 
    pte->pcd = 0; 
    pte->accessed = 0; 
    pte->dirty = 0;
    pte->pat = 0; 
    pte->global = 0; 
    pte->avl = 0; 
    pte->page_address = address;
    pte->avail = 0; 
    pte->pke = 0;
    pte->nonexecute = 0;
}

void set_pde(page_pde_t* pde_base, int n, uint64_t address, int present, int usermode){
    page_pde_t* pde = &(pde_base[n]);
    pde->present = present;
    pde->writable = 1;
    pde->usermode = usermode;
    pde->pwt = 0;
    pde->pcd = 0;
    pde->accessed = 0;
    pde->ign = 0;
    pde->o = 0;
    pde->ign = 0;
    pde->avl = 0;
    pde->page_address = address; 
    pde->avail = 0;
    pde->nonexecute = 0;
}

void set_pdpe(page_pdpe_t* pdpe_base, int n, uint64_t address, int present, int usermode){
    page_pdpe_t* pdpe = &(pdpe_base[n]);
    pdpe->present = present; 
    pdpe->writable = 1; 
    pdpe->usermode = usermode;
    pdpe->pwt = 0;
    pdpe->pcd = 0;
    pdpe->accessed = 0;
    pdpe->ign = 0;
    pdpe->o = 0;
    pdpe->ign = 0; 
    pdpe->avl = 0; 
    pdpe->page_address = address; 
    pdpe->avail = 0; 
    pdpe->nonexecute = 0;
}

void set_pml(page_pml_t* pml_base, int n, uint64_t address, int present, int usermode){
    page_pml_t* pml = &(pml_base[n]);
    pml->present = present;
    pml->writable = 1;
    pml->usermode = usermode;
    pml->pwt = 0; 
    pml->pcd = 0;
    pml->accessed = 0; 
    pml->ign = 0; 
    pml->mbz = 0;
    pml->avl = 0; 
    pml->page_address = address;
    pml->avail = 0;
    pml->nonexecute = 0;
}

#if 0 //shut up warnings when we dont need this
void debug_page_table(void* pml_ptr){
    uint64_t i, j, k, l;
    page_pml_t* pml;
    page_pdpe_t* pdpe;
    page_pde_t* pde;
    page_pte_t* pte;
    uint64_t address;
    pml = (page_pml_t*) pml_ptr;

    printf("::Printing virtual to physical mappings...::\n");
    for(i = 1; i < 512; i++){
        if (pml[i].page_address != 0){
            pdpe = (page_pdpe_t*) (pml[i].page_address << 12);

            for(j = 0; j < 512; j++){
                if (pdpe[j].page_address != 0){
                    pde = (page_pde_t*) (pdpe[j].page_address << 12);

                    for(k = 0; k < 512; k++){
                        if (pde[k].page_address != 0){
                            pte = (page_pte_t*) (pde[k].page_address << 12);

                            for(l = 0; l < 512; l++){
                                if (pte[l].page_address != 0){
                                    address = ( i << 39 | j << 30 | k << 21 | l << 12);
                                    printf("0x%llx -> 0x%llx\n", address, pte[l].page_address << 12);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
#endif

/* Create a page table for the kernel */
uint64_t create_kernel_page_table(uint64_t* base) {
    uint64_t page_addr;
    /*
     * For 4GB, we reference 1048576 physical pages
     * Using 2048 pages fro PTEs, 4 pages for PDEs, 1 for PDPE, 1 for PMLE4E
     * Total: 2054 pages = 8413184 bytes
     */

    //level 1
    //1048576/512 = 2048 pages
    page_pte_t* p = (page_pte_t*) base;
    for(int i = 0; i < 1048576; i++) { set_pte(p, i, (uint64_t)i, 1, 0); }

    /* Level 2
     * - 2048 PDE Pages to hold references to PTEs
     * - Starts at offset 1048576 from the start of the buffer
     * 2048/512 = 4 pages
     */
    page_pde_t* pde = (page_pde_t*) (base + 1048576);
    for (int i = 0; i < 2048; i++) { 
        page_addr = (uint64_t) (p + 512 * i) >> 12;
        set_pde(pde, i, page_addr, 1, 0);
    }

    /* Level 3
     * - 4 PDPE Pages to hold references to PDEs
     * - Starts at offset 1050624 from the start of the buffer
     */
    page_pdpe_t* pdpe = (page_pdpe_t*) (base + 1050624);
    for (int i = 0 ; i < 4; i++) {
        page_addr = (uint64_t) (pde + 512 * i) >> 12;
        set_pdpe(pdpe, i, page_addr, 1, 0);
    }
    // pad out the rest of the PDPE
    for (int i = 4; i < 512; i++) { set_pdpe(pdpe, i, 0, 0, 0); }

    /* Level 4
     * - 1 PML4 Page to hold references to PDPEs
     * - Starts at offset 1050624 from the start of the buffer
     */
    page_pml_t* pml = (page_pml_t*) (base + 1051136);
    set_pml(pml, 0, (uint64_t) pdpe >> 12, 1,  0);
    for (int i = 1; i < 512; i++) { set_pml(pml, i, 0, 0, 0); }

    // write location of pml into cr3 register
    // - the mask clears out the bottom 12 bits of the address
    write_cr3((uint64_t) pml);
    return (uint64_t) pml & (~0xfffULL);
};