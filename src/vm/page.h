#include <list.h>
#include "threads/palloc.h"

enum location {
  PHYS,
  DISK,
  FILE
};

struct page_entry {
  void* upage;
  int location;
  bool writable;
  struct list_elem elem;
};

struct page_entry *lookup_page(uint32_t *vaddr);
bool stack_growth(void *vaddr, bool user, bool writable);
bool allocate_page(void *vaddr, bool user, bool writable);
void push_page_table(void *upage, enum location);
void *swap_out(enum palloc_flags);
