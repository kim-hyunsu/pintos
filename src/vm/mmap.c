#include "vm/mmap.h"
#include "threads/thread.h"
#include "threads/malloc.h"

static bool order_by_mapid(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

mapid_t push_mmap_table(struct file *file) {
  struct list_elem *e;
  struct thread *t = thread_current();
  struct mmap_entry *me = NULL;
  mapid_t mapid = 0;

  list_sort(&t->mmap_table, order_by_mapid, NULL);
  for (e = list_begin(&t->mmap_table); e != list_end(&t->mmap_table); e = list_next(e)) {
    me = list_entry(e, struct mmap_entry, elem);
    if (me->mapid == mapid) {
      mapid++;
      continue;
    }
    break;
  }

  me = (struct mmap_entry *)calloc(1, sizeof(struct mmap_entry));
  me->mapid = mapid;
  me->file = file;
  list_push_back(&t->mmap_table, &me->elem);
  return mapid;
}

struct file *pop_mmap_table(mapid_t mapid) {
  struct mmap_entry *me;
  struct file *file = NULL;
  struct thread *t = thread_current();
  struct list_elem *e;

  for(e = list_begin(&t->mmap_table); e != list_end(&t->mmap_table); e = list_next(e)){
    me = list_entry(e, struct mmap_entry, elem);
    if(me->mapid == mapid){
      list_remove(&me->elem);
      file = me->file;
      free(me);
      return file;
    }
  }
  return file;
}

static bool order_by_mapid(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct mmap_entry *me_a = list_entry(a, struct mmap_entry, elem);
  struct mmap_entry *me_b = list_entry(b, struct mmap_entry, elem);
  return me_a->mapid < me_b->mapid;
}