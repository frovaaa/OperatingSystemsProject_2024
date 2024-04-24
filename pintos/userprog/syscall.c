#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f UNUSED)
{
  unsigned int bytes_read = 0;

  enum syscall_code s_code = *(enum syscall_code *)(f->esp + bytes_read);
  bytes_read += sizeof(enum syscall_code);

  switch (s_code)
  {
  case SYS_WRITE:
  {
    int fd = *(int *)(f->esp + bytes_read);
    bytes_read += sizeof(int);

    char *buffer = *(char **)(f->esp + bytes_read);
    bytes_read += sizeof(char *);

    unsigned int size = *(unsigned int *)(f->esp + bytes_read);
    bytes_read += sizeof(unsigned int);

    putbuf(buffer, size);

    break;
  }

  case SYS_EXIT:
  { // exit status is stored in the thread's exit_status
    int exit_status = *(int *)(f->esp + bytes_read);
    bytes_read += sizeof(int);

    f->eax = exit_status;

    // setting the exit status of the current thread
    *(thread_current()->exit_status) = exit_status;

    // unblocking the parent thread because the child has exited
    printf("%s: exit(%d)\n", thread_current()->name, exit_status);

    thread_unblock(thread_current()->parent);
    thread_exit();

    NOT_REACHED(); // as seen in thread.c, panic if thread cannot exit
    break;
  }

  default:
    printf("Unknown system call\n");
    thread_exit();
    NOT_REACHED();
  }
}
