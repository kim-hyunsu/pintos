#include <list.h>
#include "threads/thread.h"

struct swap_entry {
  void *frame;
  void *upage;
  struct thread *t;
  int index;
  struct list_elem elem;
};

void swap_init(void);
void read_block(void *frame, int index);
void write_block(void *frame, int index);
int push_swap_table(void *upage, void *frame, struct thread *t);
struct swap_entry *remove_swap(void *upage);
