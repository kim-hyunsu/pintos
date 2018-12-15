#include "filesys/filesys.h"
#include "filesys/cache.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/disk.h"
#include "threads/malloc.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

static bool filesys_d_lookup(struct dir *dir, const char *name, struct inode **target_inode, struct dir **target_dir, char *target_name){
  const char *delimiter = "/";
  char *dname = malloc(strlen(name)+1);
  strlcpy(dname, name, strlen(name)+1);
  char *key;
  char *save_ptr;
  bool exist = false;
  int depth = 0;
  int depth_i = 0;
  struct inode *inode;
  static char prev[NAME_MAX + 1];

  for(key = strtok_r(dname, delimiter, &save_ptr); key != NULL; key = strtok_r(NULL, delimiter, &save_ptr)){
    if(strlen(key)>14) return false;
    depth++;
  }
  strlcpy(dname, name, strlen(name)+1);

  for(key = strtok_r(dname, delimiter, &save_ptr); key != NULL; key = strtok_r(NULL, delimiter, &save_ptr)){
    depth_i++;
    if(depth_i == depth){
      exist = true;
      break;
    }
    if(!strcmp(key, ".")) continue;
    if(!strcmp(key, "..")){
      if(prev[0]=='\0') break;
      strlcpy(key, prev, NAME_MAX+1);
    }
    if(!dir_lookup(dir, key, &inode)) break;
    if(inode_is_dir(inode)){
      dir_close(dir);
      dir=dir_open(inode);
    }else{
      inode_close(inode);
    }
    strlcpy(prev, key, NAME_MAX+1);
  }
  if(!exist){
    dir=NULL;
    inode=NULL;
  }
  strlcpy(target_name, key, strlen(key)+1);
  free(dname);
  *target_dir=dir;
  *target_inode=inode;
  return dir != NULL;
}

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();

  /* For project #4 */
  c_init();

  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  /* For project #4 */
  c_flush();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_file) 
{
  /* For Project #4 */
  if(!strcmp(name, "")) return false;
  disk_sector_t inode_sector = 0;
  struct dir *root;
  if(name[0] == '/' || !thread_current()->dir){
    root = dir_open_root();
  }else{
    root = dir_reopen(thread_current()->dir);
  }

  struct dir *dir;
  struct inode *inode;
  char target[NAME_MAX+1];
  if(!filesys_d_lookup(root, name, &inode, &dir, target)) return false;
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && (is_file? inode_create (inode_sector, initial_size, 0) : dir_create(inode_sector, 200))
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  /* For Project #4 */
  if(!strcmp(name, "")) return NULL;
  struct dir *root;
  if(name[0] == '/' || !thread_current()->dir){
    root = dir_open_root();
  }else{
    root = dir_reopen(thread_current()->dir);
  }

  struct dir *dir;
  struct inode *inode;
  char target[NAME_MAX+1];
  if(!strcmp(name, "/")){
    dir = dir_open_root();
    if(root==dir) dir_close(dir);
    return (struct file*)root;
  }
  filesys_d_lookup(root, name, &inode, &dir, target);

  if (dir != NULL){
    if(!strcmp(target, "..")){
      inode = d_parent_inode(dir);
      if(!inode) return NULL;
    } else if((d_isroot(dir) && strlen(target)==0) || !strcmp(target, ".")){
      return (struct file*)dir;
    } else{
      dir_lookup(dir, target, &inode);
    }
  }

  dir_close (dir);
  if(!inode) return NULL;
  if(inode_is_dir(inode)) return (struct file*)dir_open(inode);

  return file_open (inode);
}

/* For Project #4 */
struct dir* filesys_d_open(const char *name){
  struct dir* root;
  if(name[0] == '/' || !thread_current()->dir){
    root = dir_open_root();
  } else{
    root = dir_reopen(thread_current()->dir);
  }
  struct inode *inode;
  struct dir *dir;
  char target[NAME_MAX+1];
  filesys_d_lookup(root, name, &inode, &dir, target);

  if(dir != NULL) dir_lookup(dir, target, &inode);
  dir_close(dir);
  if(!inode) return NULL;
  return dir_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  /* For Project #4 */
  if(!strcmp(name, "/")) return false;
  struct dir* root;
  if(name[0] == '/' || !thread_current()->dir){
    root = dir_open_root();
  } else{
    root = dir_reopen(thread_current()->dir);
  }

  struct inode *inode;
  struct dir *dir;
  char target[NAME_MAX+1];
  if(!filesys_d_lookup(root, name, &inode, &dir, target)) return false;

  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

bool filesys_chdir(const char *name){
  struct thread *cur = thread_current();
  if(!strcmp(name, "/")){
    if(cur->dir) dir_close(cur->dir);
    cur->dir = dir_open_root();
    return true;
  }

  struct dir* root;
  if(name[0] == '/' || !cur->dir){
    root = dir_open_root();
  } else{
    root = dir_reopen(cur->dir);
  }

  struct inode *inode;
  struct dir *dir;
  char target[NAME_MAX+1];
  if(!filesys_d_lookup(root, name, &inode, &dir, target)) return false;

  if(!strcmp(target, "..")){
    inode=d_parent_inode(dir);
    if(!inode) return false;
  } else if(!strcmp(target, ".") || (strlen(target)==0 && d_isroot(dir))){
      cur->dir = dir;
      return true;
    } else
    dir_lookup(dir, target, &inode);

  dir_close(dir);
  dir = dir_open(inode);
  if(!dir) return false;
  else{
    dir_close(cur->dir);
    cur->dir = dir;
    return true;
  }
}


/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
