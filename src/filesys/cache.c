#include "filesys/cache.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "devices/timer.h"
#include <list.h>
#include <string.h>
#include <stdio.h>

struct list c_list;
struct lock c_lock;

static void periodic_flush(void *aux UNUSED);

void c_init(void) {
	list_init(&c_list);
	lock_init(&c_lock);
	thread_create("_flusher", 0, periodic_flush, NULL);
}

static void periodic_flush(void *aux UNUSED){
	while(1){
		timer_sleep(30*TIMER_FREQ);
		c_flush();
	}
}

void c_flush(void){
	struct c_entry *ce = NULL;
	struct list_elem *e;
	lock_acquire(&c_lock);
	for(e=list_begin(&c_list); e!=list_end(&c_list); e=list_next(e)){
		ce = list_entry(e, struct c_entry, elem);
		c_disk_write(ce);
	}
	lock_release(&c_lock);
}

struct c_entry *lookup_cache(struct disk *disk, disk_sector_t sector){
	struct c_entry *ce = NULL;
	struct list_elem *e;
	for(e=list_begin(&c_list); e!=list_end(&c_list); e=list_next(e)){
		ce = list_entry(e, struct c_entry, elem);
		if(ce->disk == disk && ce->sector == sector){
			return ce;
		}
	}
	return NULL;
}

void c_push(struct c_entry *ce){
	if(list_size(&c_list) == 64) c_pop();
	list_push_back(&c_list, &ce->elem);
}

void c_pop(void){
	struct c_entry *ce = list_entry(list_pop_front(&c_list), struct c_entry, elem);
	free(ce);
}

void c_disk_write(struct c_entry *ce){
	if(ce->dirty){
		disk_write(ce->disk, ce->sector, ce->buffer);
		ce->dirty=0;
	}
}

void c_disk_read(struct c_entry *ce){
	disk_read(ce->disk, ce->sector, ce->buffer);
}

void read_cache(struct disk *disk, disk_sector_t sector, void *buffer){
	lock_acquire(&c_lock);
	struct c_entry *ce = lookup_cache(disk, sector);
	if(!ce){
		ce = (struct c_entry *)calloc(1, sizeof(struct c_entry));
		ce->disk = disk;
		ce->sector = sector;
		c_disk_read(ce);
		c_push(ce);
		ce->dirty = 0;
	}
	memcpy(buffer, ce->buffer, BUF_SIZE);
	lock_release(&c_lock);
}

void write_cache(struct disk *disk, disk_sector_t sector, void *buffer){
	lock_acquire(&c_lock);
	struct c_entry *ce = lookup_cache(disk, sector);
	if(!ce){
		ce = (struct c_entry *)calloc(1, sizeof(struct c_entry));
		ce->disk = disk;
		ce->sector = sector;
		c_push(ce);
		ce->dirty=0;
	}
	memcpy(ce->buffer, buffer, BUF_SIZE);
	ce->dirty = 1;
	c_disk_write(ce);
	lock_release(&c_lock);
}
