#include <types.h>
#include <lib.h>
#include <vm.h>
#include <mainbus.h>

struct coremap 
{
    bool in_use;
    bool is_first;
    int count;
};

void coremap_init(void);

vaddr_t get_first_free_page(int npages);

void free_page(vaddr_t v_page_addr);

bool is_initialized(void);