#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "lib/user/syscall.h"
#include "threads/interrupt.h"
#include "threads/init.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "lib/string.h"
#include "filesys/file.h"
#include "lib/kernel/console.h"
#include "devices/input.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/mmap.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);

/* For proj.#2 */
static bool compare(const struct list_elem *ea, const struct list_elem *eb, void *aux UNUSED){
	struct fd *fd1 = list_entry(ea, struct fd, elem);
	struct fd *fd2 = list_entry(eb, struct fd, elem);
	return (fd1->fd < fd2->fd);
}

static bool check_address(void *add){
	bool success = 1;
	struct thread *cur = thread_current();
	int i;
	if(!(add))
		return 0;
  if(add > PHYS_BASE - 12)
  	return 0;
  if(add < (void *)0x8048000)
  	return 0;

	for (i=0; i<4; i++){
		void *page = (void *)pagedir_get_page(cur->pagedir, add+i);
		if (add+i > PHYS_BASE-12)
			success = 0;
		if (!page) {
			success = 0;
		}
		if(add+i < (void *)CODE_BASE)
			success = 0;
		if(!(add+i))
			success = 0;
	}
	return success;
}

static bool check_right_uvaddr(void * add){
  return (add != NULL && is_user_vaddr (add));
}

static bool check_filep(const char *file){
	if(!file) 
		return 0;
	if(((int)file > 0x8084000) && ((int)file < 0x40000000)) 
		return 0;
	return 1;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  /* For proj.#2 */
  lock_init(&pair_lock);
  list_init(&pair_list);
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	thread_current()->temp_stack = f->esp;
	if(!check_address(f->esp)){
		syscall_exit(-1);
		return;
	}

	switch(*(unsigned int *)f->esp) {

		case SYS_HALT:
		{
			power_off();
			break;
		}

		case SYS_EXIT:
		{
			int status = ((int*)f->esp)[1];
			syscall_exit(status);
			break;
		}

		case SYS_EXEC:
		{
			if(!check_address(f->esp + 4)){
				syscall_exit(-1);
				break;
			}

			const char *file = (char *)(((int *)f->esp)[1]);
			if(!check_filep(file)){
				syscall_exit(-1);
				break;
			}
			tid_t tid = process_execute(file);
			f->eax = tid;
			break;
		}

		case SYS_WAIT:
		{
			tid_t tid = ((int *)f->esp)[1];
			int e_status = process_wait(tid);
			f->eax = e_status;
			break;
		}

		case SYS_CREATE:
		{
			if(!check_address(f->esp + 4)){
				syscall_exit(-1);
				break;
			}

			const char *file = (char *)(((int *)f->esp)[1]);
			if(!check_filep(file)){
				syscall_exit(-1);
				break;
			}

			off_t init_size = (off_t)((unsigned int *)f->esp)[2];

			lock_acquire(&file_lock);
			bool success = filesys_create(file, init_size, 1);
			lock_release(&file_lock);
			f->eax = 0;
			f->eax = success;
			break;
		}

		case SYS_REMOVE:
		{
			if(!check_address(f->esp + 4)){
				syscall_exit(-1);
				break;
			}

			const char *file = (char *)(((int *)f->esp)[1]);
			if(!check_filep(file)){
				syscall_exit(-1);
				break;
			}

			lock_acquire(&file_lock);
			bool success = filesys_remove(file);
			lock_release(&file_lock);
			f->eax = 0;
			f->eax = success;
			break;
		}

		case SYS_OPEN:
		{
			if(!check_address(f->esp + 4)){
				syscall_exit(-1);
				break;
			}

			const char *file = (char *)(((int *)f->esp)[1]);
			if(!check_filep(file)){
				syscall_exit(-1);
				break;
			}

			lock_acquire(&file_lock);
			int fd = syscall_open(file);
			lock_release(&file_lock);

			f->eax = fd;
			break;
		}

		case SYS_FILESIZE:
		{
			struct thread *cur = thread_current();
			struct list_elem *e;
			int fd = ((int *)f->esp)[1];
			struct fd *push_fd = NULL;

			for(e = list_begin(&cur->f_list); e != list_end(&cur->f_list); e = list_next(e)){
				push_fd = list_entry(e, struct fd, elem);
				if(push_fd->fd == fd){
					break;
				}
			}

			lock_acquire(&file_lock);
			f->eax = file_length(push_fd->file_p);
			lock_release(&file_lock);
			break;
		}

		case SYS_READ:
		{
			int fd = ((int *)f->esp)[1];
			if(!check_right_uvaddr((void *)(((int *)f->esp)[2]))){
				syscall_exit(-1);
				break;
			}

			void *b = (void *)(((int *)f->esp)[2]);
			unsigned size = ((unsigned int *)f->esp)[3];
			int read_count = syscall_read(fd, b, size);
			if(read_count == -2){
				syscall_exit(-1);
				break;
			}
			f->eax = read_count;
			break;
		}

		case SYS_WRITE:
		{
			int fd = ((int *)f->esp)[1];
			if(!check_right_uvaddr((void *)(((int *)f->esp)[2]))){
				syscall_exit(-1);
				break;
			}

			void *b = (void *)(((int *)f->esp)[2]);
			unsigned size = ((unsigned int *)f->esp)[3];
			int write_count = syscall_write(fd, b, size);
			f->eax = write_count;
			break;
		}

		case SYS_SEEK:
		{
			struct thread *cur = thread_current();
			struct list_elem *e;

			int fd = ((int *)f->esp)[1];
			off_t pos = ((off_t *)f->esp)[2];

			for(e = list_begin(&cur->f_list); e != list_end(&cur->f_list); e = list_next(e)){
				struct fd *push_fd = list_entry(e, struct fd, elem);
				if(push_fd->fd == fd){
					lock_acquire(&file_lock);
					file_seek(push_fd->file_p, pos);
					lock_release(&file_lock);
					break;
				}
			}
			break;
		}

		case SYS_TELL:
		{
			struct thread *cur = thread_current();
			struct list_elem *e;

			int fd = ((int *)f->esp)[1];
			off_t file_p = -1;

			for(e = list_begin(&cur->f_list); e != list_end(&cur->f_list); e = list_next(e)){
				struct fd *push_fd = list_entry(e, struct fd, elem);
				if(push_fd->fd == fd){
					lock_acquire(&file_lock);
					file_p = file_tell(push_fd->file_p);
					lock_release(&file_lock);
					break;
				}
			}
			f->eax = file_p;
			break;
		}

		case SYS_CLOSE:
		{
			struct thread *cur = thread_current();
			struct list_elem *e;

			int fd = ((int *)f->esp)[1];
			bool valid = 0;

			for(e = list_begin(&cur->f_list); e != list_end(&cur->f_list); e = list_next(e)){
				struct fd *push_fd = list_entry(e, struct fd, elem);
				if(push_fd->fd == fd){
					valid = 1;
					lock_acquire(&file_lock);
					if(inode_is_dir(file_get_inode(push_fd->file_p)))
						dir_close((struct dir *)push_fd->file_p);
					else
						file_close(push_fd->file_p);
					lock_release(&file_lock);

					list_remove(e);
					free(push_fd);
					break;
				}
			}
			if(!valid){
				syscall_exit(-1);
				break;
			}
			break;
		}

		case SYS_MMAP:
		{
			int fd = ((int *)f->esp)[1];

			if (!check_address(f->esp + 4)) {
				syscall_exit(-1);
				break;
			}

			void *addr = (void *)(((int *)f->esp)[2]);
			void *overlapped = lookup_page(addr);
			if (!addr || !(addr == pg_round_down(addr)) || overlapped) {
			// if (!addr || !(addr == pg_round_down(addr))) {
				f->eax = -1;
				break;
			}
			if (!is_user_vaddr(addr)) {
				syscall_exit(-1);
				break;
			}

			f->eax = (mapid_t)syscall_mmap(fd, addr);
			break;
		}

		case SYS_MUNMAP:
		{
			mapid_t mapid = (mapid_t)((int *)f->esp)[1];
			syscall_munmap(mapid);
			break;
		}

		case SYS_CHDIR:
		{
			const char *dir = (char *)(((int *)f->esp)[1]);
			f->eax = (bool)syscall_chdir(dir);
			break;
		}

		case SYS_MKDIR:
		{
			const char *dir = (char *)(((int *)f->esp)[1]);
			f->eax = (bool)syscall_mkdir(dir);
			break;
		}

		case SYS_READDIR:
		{
			int fd = ((int *)f->esp)[1];
			char *name = (char *)(((int *)f->esp)[2]);
			f->eax = (bool)syscall_readdir(fd, name);
			break;
		}

		case SYS_ISDIR:
		{
			int fd = ((int *)f->esp)[1];
			f->eax = (bool)syscall_isdir(fd);
			break;
		}

		case SYS_INUMBER:
		{
			int fd = ((int *)f->esp)[1];
			f->eax = (int)syscall_inumber(fd);
			break;
		}

		default:
		{
			syscall_exit(-1);
			break;
		}
	}
}

int syscall_exit(int status){
	struct thread *cur = thread_current();
	struct list_elem *e;
	bool is_child = 0;

	char delimeter[2] = " ";
	char *next_p;
	char *file_name = strtok_r(cur->name, delimeter, &next_p);
	struct pair *pair = NULL;

	if((status < 0) || (status > 255)) status = -1;

	lock_acquire(&pair_lock);
	for(e = list_begin(&pair_list); e != list_end(&pair_list); e = list_next(e)){
		pair = list_entry(e, struct pair, elem);
		if(cur->tid == pair->child_tid){
			pair->e_status = status;
			pair->is_exit = 1;
			is_child = 1;
			break;
		}
	}
	lock_release(&pair_lock);

	if(!is_child) status = -1;
	else sema_up(&pair->sema);
  printf("%s: exit(%d)\n", file_name, status);
  thread_exit();
}

int syscall_open(const char *file){
	struct thread *cur = thread_current();
	struct list_elem *e;
	lock_acquire(&cur->flist_lock);
	struct file *file_p = filesys_open(file);
	int i = 0;

	if(file_p == NULL) {
		lock_release(&cur->flist_lock);
		return -1;
	}

	if(list_empty(&cur->f_list)){
		struct fd *std_in = (struct fd *)malloc(sizeof(struct fd));
		struct fd *std_out = (struct fd *)malloc(sizeof(struct fd));

		std_in->fd = 0;
		std_in->file_name = "STD_IN"; 
		std_in->file_p = NULL;
    std_out->fd = 1;
    std_out->file_name = "STD_OUT";
    std_out->file_p = NULL;

		list_push_back(&cur->f_list, &std_in->elem);
		list_push_back(&cur->f_list, &std_out->elem);
	}

	struct fd *nfd = (struct fd *)malloc(sizeof(struct fd));
	for(e = list_begin(&cur->f_list); e != list_end(&cur->f_list); e = list_next(e)){
		struct fd *push_fd = list_entry(e, struct fd, elem);
		if(push_fd->fd != i){
			nfd->fd = i;
			nfd->file_name = file;
			nfd->file_p = file_p;
			list_insert_ordered(&cur->f_list, &nfd->elem, compare, NULL);
			break;
		}
		i ++;
	}

	if((size_t)i == list_size(&cur->f_list)){
		nfd->fd = i;
		nfd->file_name = file;
		nfd->file_p = file_p;
		list_push_back(&cur->f_list, &nfd->elem);
	}
	lock_release(&cur->flist_lock);

	return nfd->fd;
}

int syscall_read(int fd, void *b, unsigned size){
	timer_sleep(10);
	struct thread *cur = thread_current();
	struct list_elem *e;
	bool success = 0;
	struct fd *push_fd = NULL;

	if(fd < 0) return -2;
	if(fd == 1) return -1;
	if(fd == 0){
		uint8_t *nb = b;
		uint8_t nk;
		unsigned i = 0;

		while((nk = input_getc()) != 13){
			if(i >= size)
				break;
			*nb = nk;
			nb ++;
			i ++;
		}
		return i;
	}

	for (e = list_begin(&cur->f_list); e != list_end(&cur->f_list); e = list_next(e)){
		push_fd = list_entry(e, struct fd, elem);
		if(push_fd->fd == fd){
			success = 1;
			break;
		}
	}

	if(!success)
		return -2;

	if(inode_is_dir(file_get_inode(push_fd->file_p)))
		return -1;

	lock_acquire(&file_lock);
	int read_bytes = (int)file_read(push_fd->file_p, b, (off_t)size);
	lock_release(&file_lock);

	return read_bytes;
}

int syscall_write(int fd, void *b, unsigned size){
	struct thread *cur = thread_current();
	struct list_elem *e;
	bool success = 0;
	struct fd *push_fd = NULL;

	if(fd == 0) return -1;
	if(fd == 1){
		putbuf(b, size);
		return size;
	}

	for (e = list_begin(&cur->f_list); e != list_end(&cur->f_list); e = list_next(e)){
		push_fd = list_entry(e, struct fd, elem);
		if(push_fd->fd == fd){
			success = 1;
			break;
		}
	}

	if(!success) return -1;
	if(inode_is_dir(file_get_inode(push_fd->file_p)))
		return -1;

	lock_acquire(&file_lock);
	int write_bytes = (int)file_write(push_fd->file_p, b, (off_t)size);
	lock_release(&file_lock);
	return write_bytes;
}

mapid_t syscall_mmap(int fd, void *addr) {
	struct thread *t = thread_current();
	struct list_elem *e;
	struct fd *temp;
	struct fd *found = NULL;

	if (fd == 1 || fd == 0)
		return -1;

	for (e = list_begin(&t->f_list); e != list_end(&t->f_list); e = list_next(e)) {
		temp = list_entry(e, struct fd, elem);
		if (temp->fd == fd) {
			found = temp;
			break;
		}
	}
	if (!found)
		return -1;

	lock_acquire(&file_lock);
	struct file *file = file_reopen(found->file_p);
	lock_release(&file_lock);

	mapid_t mapid = push_mmap_table(file);

	int filesize = file_length(file);

	int count = 0;
	while (filesize > count) {
		if (pagedir_get_page(t->pagedir, addr + count))
			return -1;
		count += PGSIZE;
	}

	if (file == NULL)
		return -1;
	
	int offset = 0;
	while (filesize > 0) {
		size_t page_zero_bytes = filesize < PGSIZE ? PGSIZE-filesize : 0;    
		lazy_push_page_table(addr, file, offset, page_zero_bytes, true);
		offset += PGSIZE;
		filesize -= PGSIZE;
		addr += PGSIZE;
	}

	return mapid;
}

void syscall_munmap(mapid_t mapid) {
	struct file *file = pop_mmap_table(mapid);
	if (file == NULL)
		return;
	apply_mmap_changed(file);
}

void apply_mmap_changed(struct file *file) {
	struct thread *t = thread_current();
	struct page_entry *pe;
	struct list_elem *e;
  
	lock_acquire(&file_lock);
	file_seek(file, 0);
	for (e = list_begin(&t->page_table); e != list_end(&t->page_table); e = list_next(e)) {
		pe = list_entry(e, struct page_entry, elem);
		if (pe->file == file) {
			if (pagedir_is_dirty(t->pagedir, pe->upage)) {
				file_write(file, pe->upage, PGSIZE - pe->page_zero_bytes);
			}
			if (pe->location == FILE) {
				list_remove(&pe->elem);
				free(pe);
			}
		}
	}
	lock_release(&file_lock);
}

static struct fd *lookup_fd(int fd){
	struct list *f_list = &thread_current()->f_list;
	struct list_elem *e;
	struct fd *found = NULL;
	struct fd *match = NULL;

	for (e=list_begin(f_list); e!=list_end(f_list); e=list_next(e)){
		found = list_entry(e, struct fd, elem);
		if(found->fd == fd){
			match = found;
			break;
		}
	}
	return match;
}

bool syscall_chdir(const char *dir){
	bool success;
	lock_acquire(&file_lock);
	success = filesys_chdir(dir);
	lock_release(&file_lock);
	return success;
}

bool syscall_mkdir(const char *dir){
	bool success;
	lock_acquire(&file_lock);
	success = filesys_create(dir, 0, false);
	lock_release(&file_lock);
	return success;
}

bool syscall_readdir(int fd, char name[READDIR_MAX_LEN + 1]){
	struct list_elem *e;
	struct fd *found = NULL;
	struct thread *cur = thread_current();
	bool success = 0;

	for(e=list_begin(&cur->f_list); e!=list_end(&cur->f_list); e=list_next(e)){
		found = list_entry(e, struct fd, elem);
		if(found->fd == fd){
			success = 1;
			break;
		}
	}
	if(!success) return -1;

	struct inode *inode = file_get_inode(found->file_p);
	if(inode == NULL) return 0;
	if(!inode_is_dir(inode)) return 0;
	struct dir *dir = (struct dir *)found->file_p;
	if(!dir_readdir(dir, name)) return 0;

	return 1;
}

bool syscall_isdir(int fd){
	struct fd *_fd = lookup_fd(fd);
	if(!_fd) return -1;
	return inode_is_dir(file_get_inode((struct file *)_fd->file_p));
}

int syscall_inumber(int fd){
	struct fd *_fd = lookup_fd(fd);
	if(!_fd) return -1;
	return inode_get_inumber(file_get_inode((struct file *)_fd->file_p));
}
