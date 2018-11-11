#include <list.h>
#include "devices/disk.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "vm/swap.h"

static struct disk *swap_disk;
static struct lock swap_disk_lock;
static struct list swap_table;
static struct lock swap_table_lock;
static bool order_by_index(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

void swap_init(void) {
  lock_init(&swap_disk_lock);
  swap_disk = disk_get(1, 1);
  lock_init(&swap_table_lock);
  list_init(&swap_table);
}

void read_disk(void *frame, int index) {
  int i;
  lock_acquire(&swap_disk_lock);
  for (i = 0; i < 8; i++) {
    disk_read(swap_disk, index + i, (void *)frame + (i * DISK_SECTOR_SIZE));
  }
  lock_release(&swap_disk_lock);
}

void write_disk(void *frame, int index) {
  int i;
  lock_acquire(&swap_disk_lock);
  for (i = 0; i < 8; i++) {
    disk_write(swap_disk, index + i, (void *)frame + (i * DISK_SECTOR_SIZE));
  }
  lock_release(&swap_disk_lock);
}

int push_swap_table(void *upage, void *frame, struct thread *t) {
	int index = 0;
	struct list_elem *e;
	struct swap_entry *temp;
  struct swap_entry *se;
	lock_acquire(&swap_table_lock);
	list_sort(&swap_table, &order_by_index, NULL);
	for(e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)){
		temp = list_entry(e, struct swap_entry, elem);
		if(temp->index == index){
			index += 8;
			continue;
		}
		break;
	}
  se = calloc(1, sizeof(struct swap_entry));
  se->upage = upage;
  se->frame = frame;
  se->t = t;
  se->index = index;
  list_push_back(&swap_table, &se->elem);
  lock_release(&swap_table_lock);
  return index;
}

int remove_swap(void *upage) {
  struct list_elem *e;
  struct swap_entry *se;
  int index = 0;
  lock_acquire(&swap_table_lock);
  for (e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)) {
    se = list_entry(e, struct swap_entry, elem);
    if (se->upage == upage) {
      list_remove(&se->elem);
      index = se->index;
      free(se);
      break;
    }
  }
  lock_release(&swap_table_lock);
  return index;
}

static bool order_by_index(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct swap_entry *se_a = list_entry(a, struct swap_entry, elem);
  struct swap_entry *se_b = list_entry(b, struct swap_entry, elem);
  return se_a->index < se_b->index;
}
