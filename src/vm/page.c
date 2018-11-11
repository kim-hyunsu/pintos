#include <list.h>
#include <stdbool.h>
#include <stdio.h>
#include "threads/thread.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/off_t.h"

static bool install_page (void *upage, void *frame, bool writable);
static void swap_in(void *upage, void *frame);
static void change_location(void *upage, enum location);

struct page_entry *lookup_page(uint32_t *vaddr) {
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
  enum palloc_flags flag = user ? PAL_USER|PAL_ZERO : PAL_ZERO;
  void *frame = palloc_get_page(flag);
  if (!frame)
    frame = swap_out(flag);
  if (!install_page(upage, frame, writable)) {
    palloc_free_page(frame);
    return false;
  }
  push_page_table(upage, PHYS);
  push_frame_table(upage, frame, thread_current());
  return true;
}

bool allocate_page(void *vaddr, bool user, bool writable) {
  void *upage = pg_round_down(vaddr);
  enum palloc_flags flag = user ? PAL_USER|PAL_ZERO : PAL_ZERO;
  void *frame = palloc_get_page(flag);
  if (!frame)
    frame = swap_out(flag);
  if (!install_page(upage, frame, writable)) {
    palloc_free_page(frame);
    return false;
  }
  change_location(upage, PHYS);
  swap_in(upage, frame);
  return true;
}

bool lazy_loading(void *vaddr, bool user, bool writable, struct file *file, off_t offset, size_t page_zero_bytes) {
  void *upage = pg_round_down(vaddr);
  enum palloc_flags flag = user ? PAL_USER|PAL_ZERO : PAL_ZERO;
  void *frame = palloc_get_page(flag);
  if (!frame)
    frame = swap_out(flag);
  size_t page_read_bytes = PGSIZE - page_zero_bytes;
  file_seek(file, offset);
  if (file_read(file, frame, page_read_bytes) != (int)page_read_bytes) {
    palloc_free_page(frame);
    return false;
  }
  if (!install_page(upage, frame, writable)) {
    palloc_free_page(frame);
    return false;
  }
  change_location(upage, PHYS);
  push_frame_table(upage, frame, thread_current());
  return true;
}

void push_page_table(void *upage, enum location location) {
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

void lazy_push_page_table(void *upage, struct file *file, off_t offset, size_t page_zero_bytes, bool writable) {
  struct thread *t = thread_current();
  struct page_entry *pe = lookup_page(upage);
  if (pe) {
    pe->location = FILE;
    pe->is_lazy = true;
    pe->file = file;
    pe->offset = offset;
    pe->page_zero_bytes = page_zero_bytes;
    pe->writable = writable;
    return;
  }
  pe = calloc(1, sizeof(struct page_entry));
  pe->upage = upage;
  pe->location = FILE;
  pe->is_lazy = true;
  pe->file = file;
  pe->offset = offset;
  pe->page_zero_bytes = page_zero_bytes;
  pe->writable = writable;
  list_push_back(&t->page_table, &pe->elem);
}

void *swap_out(enum palloc_flags flags) {
  struct thread *t = thread_current();
  struct frame_entry *fe = pop_frame_table();
  struct page_entry *pe = lookup_page(fe->upage);
  pe->location = DISK;
  int index = push_swap_table(fe->upage, fe->frame, t);
  write_disk(fe->frame, index);
  pagedir_clear_page(t->pagedir, fe->upage);
  palloc_free_page(fe->frame);
  free(fe);
  return palloc_get_page(flags);
}

static bool install_page (void *upage, void *frame, bool writable) {
  struct thread *t = thread_current ();
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, frame, writable));
}

static void swap_in(void *upage, void *frame) {
  int index = remove_swap(upage);
  read_disk(frame, index);
  push_frame_table(upage, frame, thread_current());
}

static void change_location(void *upage, enum location location) {
  struct page_entry *pe = lookup_page(upage);
  pe->location = location;
}
