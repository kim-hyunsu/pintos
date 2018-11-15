#include <list.h>
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/off_t.h"

enum location {
  PHYS,
  DISK,
  FILE,
  STACK
};

struct page_entry {
  void* upage;
  int location;
  bool is_lazy;
  struct file *file;
  off_t offset;
  size_t page_zero_bytes;
  bool writable;
  struct list_elem elem;
  void **esp;
};

struct page_entry *lookup_page(uint32_t *vaddr);
bool stack_growth(void *vaddr, bool user, bool writable);
bool allocate_page(void *vaddr, bool user, bool writable);
bool lazy_loading(void *vaddr, bool user, bool writable, struct file *file, off_t offset, size_t page_zero_bytes);
void push_page_table(void *upage, enum location);
void lazy_push_page_table(void *upage, struct file *file, off_t offset, size_t page_zero_bytes, bool writable);
