#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"
#include "threads/synch.h"

void syscall_init (void);

#define CODE_BASE 0x8048000

// Project #2
struct pair
{
	struct thread *parent;
	tid_t child_tid;
	bool success;
	bool is_exit;
	int e_status;
	struct list_elem elem;
	struct semaphore sema;
	struct semaphore l_sema;
};

struct list pair_list;
struct lock pair_lock;
struct lock file_lock;

int syscall_exit(int status);
int syscall_open(const char *file);
int syscall_read(int fd, void *buffer, unsigned size);
int syscall_write(int fd, void *buffer, unsigned size);

#endif /* userprog/syscall.h */
