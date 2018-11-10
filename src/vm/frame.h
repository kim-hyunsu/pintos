#include <list.h>
#include "threads/thread.h"

struct frame_entry {
  void* frame;
  void* upage;
  struct thread *t;
  struct list_elem elem;
};

void frame_init(void);
void push_frame_table(void *upage, void* frame, struct thread *t);
struct frame_entry *pop_frame_table(void);
