#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdio.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

// For Proejct #4
#define DISK_NUM 14
#define MAX_SIZE 8*1024*8192
#define INIT_SECTOR 0xffffffff

#define D_DISKS 12
#define ID_DISKS 128

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    // disk_sector_t start;                /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */    
    // uint32_t unused[125];               /* Not used. */
    uint32_t unused[107];
    int isdir;
    uint32_t d_index;
    uint32_t id_index;
    uint32_t d_id_index;
    disk_sector_t parent;
    disk_sector_t disks[DISK_NUM];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

static size_t check_id_disk(off_t size){
  if(size <= DISK_SECTOR_SIZE*D_DISKS) return 0;
  size -= DISK_SECTOR_SIZE*D_DISKS;
  return DIV_ROUND_UP(size, DISK_SECTOR_SIZE*ID_DISKS);
}

static size_t check_d_id_disk(off_t size){
  if(size <= DISK_SECTOR_SIZE*D_DISKS + DISK_SECTOR_SIZE*ID_DISKS) return 0;
  return 1;
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    // struct inode_disk data;             /* Inode content. */

    //For Project #4
    off_t length;
    off_t r_length;
    uint32_t d_index;
    uint32_t id_index;
    uint32_t d_id_index;

    struct lock ilock;
    int isdir;
    disk_sector_t parent;
    disk_sector_t disks[DISK_NUM];
  };

struct id_disk{
  disk_sector_t disks[ID_DISKS];
};

bool disk_alloc(struct inode_disk *disk_inode);
void disk_dalloc(struct inode *inode);
void id_dalloc(disk_sector_t *disks, size_t id_sectors);
void d_id_dalloc(disk_sector_t *disks, size_t id_disk, size_t sectors);  
size_t d_disk_extend(struct inode *inode, size_t sectors);
size_t id_disk_extend(struct inode *inode, size_t sectors);
size_t d_id_disk_extend(struct inode *inode, size_t sectors, struct id_disk *id_disk);
off_t inode_extend(struct inode *inode, off_t length);

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t length, off_t pos) 
{
  ASSERT (inode != NULL);

  //For Project #4
  if(pos < length){
    int i;
    disk_sector_t id_disk[ID_DISKS];
    if(pos < DISK_SECTOR_SIZE*D_DISKS) return inode->disks[pos/DISK_SECTOR_SIZE];
    else if(pos < (DISK_SECTOR_SIZE*D_DISKS + DISK_SECTOR_SIZE*ID_DISKS)){
    pos -= DISK_SECTOR_SIZE*D_DISKS;
    i = pos/(DISK_SECTOR_SIZE*ID_DISKS) + D_DISKS;
    disk_read(filesys_disk, inode->disks[i], &id_disk);
    pos = pos%(DISK_SECTOR_SIZE*ID_DISKS);
    return id_disk[pos/DISK_SECTOR_SIZE];
  } else{
    disk_read(filesys_disk, inode->disks[D_DISKS+1], &id_disk);
    pos -= (DISK_SECTOR_SIZE*D_DISKS + DISK_SECTOR_SIZE*ID_DISKS);
    i = pos/(DISK_SECTOR_SIZE*ID_DISKS);
    disk_read(filesys_disk, id_disk[i], &id_disk);
    pos = pos%(DISK_SECTOR_SIZE*ID_DISKS);
    return id_disk[pos/DISK_SECTOR_SIZE];
  }
} else return -1;

  // if (pos < inode->data.length)
  //   return inode->data.start + pos / DISK_SECTOR_SIZE;
  // else
  //   return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

//For Project #4
bool disk_alloc(struct inode_disk *disk_inode){
  struct inode inode;
  inode.length = 0;
  inode.d_index = 0;
  inode.id_index = 0;
  inode.d_id_index = 0;

  inode_extend(&inode, disk_inode->length);
  disk_inode->d_index = inode.d_index;
  disk_inode->id_index = inode.id_index;
  disk_inode->d_id_index = inode.d_id_index;
  memcpy(&disk_inode->disks, &inode.disks, sizeof(disk_sector_t)*DISK_NUM);
  return true;
}

void disk_dalloc(struct inode *inode){
  size_t sectors = bytes_to_sectors(inode->length);
  size_t id_disk = check_id_disk(inode->length);
  size_t d_id_disk = check_d_id_disk(inode->length);
  uint32_t i = 0;

  while(sectors && i < D_DISKS){
    free_map_release(inode->disks[i], 1);
    sectors--;
    i++;
  }
  while(id_disk && i < D_DISKS+1){
    size_t id_sectors = sectors < ID_DISKS ? sectors : ID_DISKS;
    id_dalloc(&inode->disks[i], id_sectors);
    sectors -= id_sectors;
    id_disk--;
    i++;
  }
  if(d_id_disk) d_id_dalloc(&inode->disks[i], id_disk, sectors);
}

void id_dalloc(disk_sector_t *disks, size_t id_sectors){
  uint32_t i = 0;
  struct id_disk id_disk;
  disk_read(filesys_disk, *disks, &id_disk);
  while(i < id_sectors){
    free_map_release(id_disk.disks[i], 1);
    i++;
  }
  free_map_release(*disks, 1);
}

void d_id_dalloc(disk_sector_t *disks, size_t id_disk, size_t sectors){
  uint32_t i = 0;
  struct id_disk d_id_disk;
  disk_read(filesys_disk, *disks, &d_id_disk);
  while(i < id_disk){
    size_t d_id_sectors = sectors < ID_DISKS ? sectors : ID_DISKS;
    id_dalloc(&d_id_disk.disks[i], d_id_sectors);
    sectors -= d_id_sectors;
  }
  free_map_release(*disks, 1);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, int isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      // size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;

      //For Project #4
      disk_inode->isdir = isdir;
      disk_inode->parent = ROOT_DIR_SECTOR;
      if(disk_alloc(disk_inode)){
        disk_write(filesys_disk, sector, disk_inode);
        success = true;
      }

      // if (free_map_allocate (sectors, &disk_inode->start))
      //   {
      //     disk_write (filesys_disk, sector, disk_inode);
      //     if (sectors > 0) 
      //       {
      //         static char zeros[DISK_SECTOR_SIZE];
      //         size_t i;
              
      //         for (i = 0; i < sectors; i++) 
      //           disk_write (filesys_disk, disk_inode->start + i, zeros); 
      //       }
      //     success = true; 
      //   }

      free (disk_inode);
    }
  return success;
}

//For Proejct #4
bool inode_is_dir(struct inode *inode){
  return inode->isdir;
}

int inode_get_open_cnt(const struct inode *inode){
  return inode->open_cnt;
}

disk_sector_t inode_get_parent(const struct inode *inode){
  return inode->parent;
}

bool inode_set_parent(disk_sector_t parent, disk_sector_t child){
  struct inode *inode = inode_open(child);
  if(!inode) return false;
  inode->parent = parent;
  inode_close(inode);
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  lock_init(&inode->ilock);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  struct inode_disk disk_inode;
  disk_read (filesys_disk, inode->sector, &disk_inode);
  inode->d_index = disk_inode.d_index;
  inode->id_index = disk_inode.id_index;
  inode->d_id_index = disk_inode.d_id_index;
  inode->length = disk_inode.length;
  inode->r_length = disk_inode.length;
  inode->isdir = disk_inode.isdir;
  inode->parent = disk_inode.parent;

  memcpy(&inode->disks, &disk_inode.disks, sizeof(disk_sector_t)*DISK_NUM);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed){
        free_map_release (inode->sector, 1);
        // free_map_release (inode->data.start,
        //                   bytes_to_sectors (inode->data.length)); 

        //For Project #4
        disk_dalloc(inode);
      } else {
        struct inode_disk disk_inode;
        disk_inode.d_index = inode->d_index;
        disk_inode.id_index = inode->id_index;
        disk_inode.d_id_index = inode->d_id_index;
        disk_inode.length = inode->length;
        disk_inode.magic = INODE_MAGIC;
        disk_inode.isdir = inode->isdir;
        disk_inode.parent = inode->parent;

        memcpy(&disk_inode.disks, &inode->disks, sizeof(disk_sector_t)*DISK_NUM);
        disk_write(filesys_disk, inode->sector, &disk_inode);
      }
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  //For Project #4
  off_t r_length = inode->r_length;

  if(offset >= r_length) return bytes_read;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, r_length, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Read full sector directly into caller's buffer. */
          disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          disk_read (filesys_disk, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  //For Project #4
  if(offset + size > inode_length(inode)){
    if(!inode->isdir) lock_acquire(&inode->ilock);
    inode->length = inode_extend(inode, offset + size);
    if(!inode->isdir) lock_release(&inode->ilock);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, inode_length(inode), offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          /* Write full sector directly to disk. */
          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            disk_read (filesys_disk, sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          disk_write (filesys_disk, sector_idx, bounce); 
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);
  inode->r_length = inode_length(inode);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}

size_t d_disk_extend(struct inode *inode, size_t sectors){
  static char zero[DISK_SECTOR_SIZE];
  struct id_disk id_disk;
  if(inode->id_index == 0) free_map_allocate(1, &inode->disks[inode->d_index]);
  else disk_read(filesys_disk, inode->disks[inode->d_index], &id_disk);

  while(inode->id_index < ID_DISKS){
    free_map_allocate(1, &id_disk.disks[inode->id_index]);
    disk_write(filesys_disk, id_disk.disks[inode->id_index], zero);
    inode->id_index++;
    sectors--;
    if(sectors == 0) break;
  }

  disk_write(filesys_disk, inode->disks[inode->d_index], &id_disk);
  if(inode->id_index == ID_DISKS){
    inode->id_index = 0;
    inode->d_index++;
  }
  return sectors;
}

size_t id_disk_extend(struct inode *inode, size_t sectors){
  struct id_disk id_disk;
  if(inode->id_index == 0 && inode->d_id_index == 0) free_map_allocate(1, &inode->disks[inode->d_index]);
  else disk_read(filesys_disk, inode->disks[inode->d_index], &id_disk);

  while(inode->id_index < ID_DISKS){
    sectors = d_id_disk_extend(inode, sectors, &id_disk);
    if(sectors == 0) break;
  }

  disk_write(filesys_disk, inode->disks[inode->d_index], &id_disk);
  return sectors;
}

size_t d_id_disk_extend(struct inode *inode, size_t sectors, struct id_disk *id_disk){
  static char zero[DISK_SECTOR_SIZE];
  struct id_disk d_id_disk;
  if(inode->d_id_index == 0) free_map_allocate(1, &id_disk->disks[inode->id_index]);
  else disk_read(filesys_disk, id_disk->disks[inode->id_index], &d_id_disk);

  while(inode->d_id_index < ID_DISKS){
    free_map_allocate(1, &d_id_disk.disks[inode->d_id_index]);
    disk_write(filesys_disk, d_id_disk.disks[inode->d_id_index], zero);
    inode->d_id_index++;
    sectors--;
    if(sectors == 0) break;
  }

  disk_write(filesys_disk, id_disk->disks[inode->id_index], &d_id_disk);
  if(inode->d_id_index == ID_DISKS){
    inode->d_id_index = 0;
    inode->id_index++;
  }
  return sectors;
}

off_t inode_extend(struct inode *inode, off_t length){
  static char zero[DISK_SECTOR_SIZE];
  size_t sectors = bytes_to_sectors(length) - bytes_to_sectors(inode->length);

  if(sectors == 0) return length;
  while(inode->d_index < D_DISKS){
    free_map_allocate(1, &inode->disks[inode->d_index]);
    disk_write(filesys_disk, inode->disks[inode->d_index], zero);
    inode->d_index++;
    sectors--;
    if(sectors == 0) return length;
  } if(inode->d_index == D_DISKS){
    sectors = d_disk_extend(inode, sectors);
    if(sectors == 0) return length;
  } if(inode->d_index == D_DISKS+1){
    sectors = id_disk_extend(inode, sectors);
  }
  return length - sectors*DISK_SECTOR_SIZE;
}
