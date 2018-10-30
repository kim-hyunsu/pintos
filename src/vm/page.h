#include <list.h>

enum {
  PHYS
  DISK
  FILE
};

struct page_entry {
  void* upage;
  int location;
  bool writable;
  struct list_elem elem;
};

struct page_entry lookup_page(uint32_t *vaddr);
bool stack_growth(void *vaddr, bool user, bool writable);
