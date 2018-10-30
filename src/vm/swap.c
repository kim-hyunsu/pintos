#include <list.h>
#include "threads/thread.h"
#include "vm/swap.h"

static struct block *swap_block;
static struct lock swap_block_lock;
static struct list swap_table;
static struct lock swap_table_lock;
static bool order_by_index(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

void swap_init(void) {
  lock_init(&swap_block_lock);
  swap_block = block_get_role(BLOCK_SWAP);
  lock_init(&swap_table_lock);
  list_init(&swap_table);
}

void read_block(void *frame, int index) {
  int i;
  lock_acquire(&swap_block_lock);
  for (i = 0; i < 8; i++) {
    block_read(swap_block, index + i, (void *)frame + (i * BLOCK_SECTOR_SIZE));
  }
  lock_release(&swap_block_lock);
}

void write_block(void *frame, int index) {
  int i;
  lock_acquire(&swap_block_lock);
  for (i = 0; i < 8; i++) {
    block_write(swap_block, index + i, (void *)frame + (i * BLOCK_SECTOR_SIZE));
  }
  lock_release(&swap_block_lock);
}

void push_swap_table(void *upage, void *frame, struct thread *t, int index) {
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
}

struct frame_entry remove_swap(void *upage) {
  struct list_elem *e;
  struct swap_entry *se;
  struct swap_entry *found = NULL;
  lock_acquire(&swap_table_lock);
  for (e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)) {
    se = list_entry(e, struct swap_entry, elem);
    if (se->upage == upage) {
      found = se;
      break;
    }
  }
  lock_release(&swap_table_lock);
  return found;
}

static bool order_by_index(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct swap_entry *se_a = list_entry(a, struct swap_entry, elem);
  struct swap_entry *se_b = list_entry(b, struct swap_entry, elem);
  return se_a->index < se_b->index;
}