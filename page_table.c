#include<page_table.h>
#include<printf.h>
#include <allocator.h>

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

void get_page_indexes(page_table_indexer_t* indexes, void* virtual_addr){
    uint64_t addr = (uint64_t) virtual_addr;
    indexes->pte_idx =  (addr >> 12) & PT_INDEX_MASK;
    indexes->pde_idx =  (addr >> 21) & PT_INDEX_MASK;
    indexes->pdpe_idx = (addr >> 30) & PT_INDEX_MASK;
    indexes->pml_idx =  (addr >> 39) & PT_INDEX_MASK;
}

void clear_page(void* addr){
    __builtin_memset(addr, 0, PAGESIZE);
}

int map_memory(page_pml_t* pml4, void* virtual_addr, void* physical_addr){
    page_table_indexer_t indexes;
    void* temp_addr;

    get_page_indexes(&indexes, virtual_addr);

    if(!pml4)
        return 1;

    if(!pml4[indexes.pml_idx].present){
        //create entry
        if(!(temp_addr = request_page()))
            return 2;
        clear_page(temp_addr);
        set_pml(pml4, indexes.pml_idx, (uint64_t)temp_addr >> PAGESHIFT, 1, 0);
    }
    page_pdpe_t* pdpe = (page_pdpe_t*)((uint64_t)(pml4[indexes.pml_idx].page_address) << PAGESHIFT);
    if(!pdpe[indexes.pdpe_idx].present){
        //create entry
        if(!(temp_addr = request_page()))
            return 3;
        clear_page(temp_addr);
        set_pdpe(pdpe, indexes.pdpe_idx, (uint64_t)temp_addr >> PAGESHIFT, 1, 0);
    }
    page_pde_t* pde = (page_pde_t*)((uint64_t)(pdpe[indexes.pdpe_idx].page_address) << PAGESHIFT);
    if(!pde[indexes.pde_idx].present){
        //create entry
        if(!(temp_addr = request_page()))
            return 4;
        clear_page(temp_addr);
        set_pde(pde, indexes.pde_idx, (uint64_t)temp_addr >> PAGESHIFT, 1, 0);
    }
    page_pte_t* pte = (page_pte_t*)((uint64_t)(pde[indexes.pde_idx].page_address) << PAGESHIFT);
    set_pte(pte, indexes.pte_idx, (uint64_t)physical_addr >> PAGESHIFT, 1, 0);
    return 0;
}


//shut up warnings when we dont need this
void debug_page_table(void* pml_ptr){
    uint64_t i, j, k, l;
    page_pml_t* pml;
    page_pdpe_t* pdpe;
    page_pde_t* pde;
    page_pte_t* pte;
    uint64_t address;
    pml = (page_pml_t*) pml_ptr;

    printf("::Printing virtual to physical mappings...::\n");
    for(i = 0; i < 512; i++){
        if (pml[i].present){
            pdpe = (page_pdpe_t*) ((uint64_t)pml[i].page_address << PAGESHIFT);

            for(j = 0; j < 512; j++){
                if (pdpe[j].present){
                    pde = (page_pde_t*) ((uint64_t)pdpe[j].page_address << PAGESHIFT);

                    for(k = 0; k < 512; k++){
                        if (pde[k].present){
                            pte = (page_pte_t*) ((uint64_t)pde[k].page_address << PAGESHIFT);

                            for(l = 0; l < 512; l++){
                                if (pte[l].present){
                                    address = ( i << 39 | j << 30 | k << 21 | l << 12);
                                    printf("0x%llx -> 0x%llx\n", address, pte[l].page_address << PAGESHIFT);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
