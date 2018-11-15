#include <list.h>
#include "lib/user/syscall.h"

struct mmap_entry {
  mapid_t mapid;
  struct file *file;
  struct list_elem elem;
};

struct file *pop_mmap_table(mapid_t mapid);
mapid_t push_mmap_table(struct file *file);
