#include "devices/disk.h"
#include "threads/synch.h"
#include <list.h>

#define BUF_SIZE 512

struct c_entry {
	struct list_elem elem;
	struct disk *disk;
	disk_sector_t sector;
	bool dirty;
	char buffer[BUF_SIZE];
};

void c_init(void);
void c_flush(void);
struct c_entry *lookup_cache(struct disk *disk, disk_sector_t sector);
void c_push(struct c_entry *ce);
void c_pop(void);
void c_disk_read(struct c_entry *ce);
void c_disk_write(struct c_entry *ce);
void read_cache(struct disk *disk, disk_sector_t sector, void *buffer);
void write_cache(struct disk *disk, disk_sector_t sector, void *buffer);
