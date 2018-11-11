#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/init.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "lib/string.h"
#include "filesys/file.h"
#include "lib/kernel/console.h"
#include "devices/input.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"

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
			bool success = filesys_create(file, init_size);
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

	lock_acquire(&file_lock);
	int write_bytes = (int)file_write(push_fd->file_p, b, (off_t)size);
	lock_release(&file_lock);
	return write_bytes;
}
