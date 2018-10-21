#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
static void syscall_exec(void *esp, uint32_t *eax);
static void syscall_wait(void *esp, uint32_t *eax);
static void syscall_create(void *esp, uint32_t *eax);
static void syscall_remove(void *esp, uint32_t *eax);
static void syscall_open(void *esp, uint32_t *eax);
static void syscall_filesys(void *esp, uint32_t *eax);
static void syscall_read(void *esp, uint32_t *eax);
static void syscall_write(void *esp, uint32_t *eax);
static void syscall_seek(void *esp, uint32_t *eax);
static void syscall_tell(void *esp, uint32_t *eax);
static void syscall_close(void *esp, uint32_t *eax);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  switch(*(int *)f->esp) {
  case SYS_HALT: { shutdown_power_off(); break; }
  case SYS_EXIT: { syscall_exit(((int *)f->esp)[1]); break; }
  case SYS_EXEC: { syscall_exec(f->esp, &f->eax); break; }
  case SYS_WAIT: { syscall_wait(f->esp, &f->eax); break; }
  case SYS_CREATE: { syscall_create(f->esp, &f->eax); break; }
  case SYS_REMOVE: { syscall_remove(f->esp, &f->eax); break; }
  case SYS_OPEN: { syscall_open(f->esp, &f->eax); break; }
  case SYS_FILESIZE: { syscall_filesize(f->esp, &f->eax); break; }
  case SYS_READ: { syscall_read(f->esp, &f->eax); break; }
  case SYS_WRITE: { syscall_write(f->esp, &f->eax); break; }
  case SYS_SEEK: { syscall_seek(f->esp, &f->eax); break; }
  case SYS_TELL: { syscall_tell(f->esp, &f->eax); break; }
  case SYS_CLOSE: { syscall_close(f->esp, &f->eax); break; }
  default: thread_exit();
  }
}

static void syscall_exit(int status) {

}

static void syscall_exec(void *esp, uint32_t *eax) {

}

static void syscall_wait(void *esp, uint32_t *eax) {

}

static void syscall_create(void *esp, uint32_t *eax) {

}

static void syscall_remove(void *esp, uint32_t *eax) {

}

static void syscall_open(void *esp, uint32_t *eax) {

}

static void syscall_filesys(void *esp, uint32_t *eax) {

}

static void syscall_read(void *esp, uint32_t *eax) {

}

static void syscall_write(void *esp, uint32_t *eax) {

}

static void syscall_seek(void *esp, uint32_t *eax) {

}

static void syscall_tell(void *esp, uint32_t *eax) {

}

static void syscall_close(void *esp, uint32_t *eax) {

}