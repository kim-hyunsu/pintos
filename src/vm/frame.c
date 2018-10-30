#include <list.h>
#include "threads/synch.h"
#include "threads/thread.h"
#include "vm/frame.h"

static struct list frame_table;
static struct lock frame_table_lock;

void frame_init(void) {
  lock_init(&frame_table_lock);
  list_init(&frame_table);
}

void push_frame_table(void *upage, void* frame, struct thread *t) {
  struct frame_entry *fe = calloc(1, sizeof(struct frame_entry));
  fe->frame = frame;
  fe->upage = upage;
  fe->t = t;
  lock_acquire(&frame_table_lock);
  list_push_back(&frame_table, &fe->elem);
  lock_release(&frame_table_lock);  
}

struct frame_entry pop_frame_table(void) {
  struct frame_entry *fe = NULL;
  lock_acquire(&frame_table_lock);
  fe = list_entry(list_pop_front(&frame_table), struct frame_entry, elem);
  lock_release(&frame_table_lock);
  return fe;
}

