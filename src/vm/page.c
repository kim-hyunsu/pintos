#include <list.h>
#include "threads/thread.h"
#include "vm/page.h"

static bool install_page (void *upage, void *frame, bool writable);
static void push_page_table(void *upage, enum location);
static void *swap_out(enum palloc_flags flags);
static void swap_in(void *upage, frame);

struct page_entry lookup_page(uint32_t *vaddr) {
  void *upage = pg_round_down(vaddr);
  struct thread *t = thread_current();
  struct list_elem *e;
  struct page_entry *pe;
  struct page_entry *found = NULL;
  for (e = list_begin(&t->page_table); e != list_end(&t->page_table); e = list_next(e)) {
    pe = list_entry(e, struct page_entry, elem);
    if (pe->upage == upage) {
      found = pe;
      break;
    }
  }
  return found;
}

bool stack_growth(void *vaddr, bool user, bool writable) {
  void *upage = pg_round_down(vaddr);
  void *frame = palloc_get_page(user ? PAL_USER|PAL_ZERO : PAL_ZERO);
  if (!frame) {
    // printf("Not enough physical memory for stack growth.\n");
    // palloc_free_page(frame);
    // return 0;
    frame = swap_out(flag);
  }
  if (!install_page(upage, frame, writable)) {
    palloc_free_page(frame);
    return 0;
  }
  push_page_table(upage, PHYS);
  push_frame_table(upage, frame, thread_current());
  return 1;
}

bool allocate_page(void *vaddr, bool user, bool writable) {
  void *upage = pg_round_down(vaddr);
  enum palloc_flags flag = user ? PAL_USER|PAL_ZERO : PAL_ZERO;
  void *frame = palloc_get_page(flag);
  if (!frame)
    frame = swap_out(flag);
  swap_in(upage, frame)
  if (!install_page(upage, frame, writable)) {
    palloc_free_page(frame);
    return 0;
  }
  push_page_table(upage, PHYS);
  push_frame_table(upage, frame, thread_current());
  return 1;
}

static bool install_page (void *upage, void *frame, bool writable) {
  struct thread *t = thread_current ();
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, frame, writable));
}

static void push_page_table(void *upage, enum location) {
  struct thread *t = thread_current();
  struct page_entry *pe = lookup_page(upage);
  if (pe) {
    pe->location = location;
    return;
  }
  pe = calloc(1, sizeof(struct page_entry));
  pe->upage = upage;
  pe->location = location;
  list_push_back(&t->page_table, &pe->elem);
}

static void *swap_out(enum palloc_flags flags) {

}

static void swap_in(void *upage, frame) {

}