#include <types.h>
#include <lib.h>
#include <vm.h>
#include <mainbus.h>

struct page_table{
    struct page_table_obj *pages[1000];
};

struct page_table_obj{
    vaddr_t virtual_address;
    paddr_t physical_address;
};

struct core_map{
    struct lock *p_addr_lk;
    paddr_t **p_array;
    bool initialized;
};