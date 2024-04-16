#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void * esp = f->esp + sizeof(enum syscall_code);
  enum syscall_code s_code = *(enum syscall_code *)esp;

  switch (s_code)
  {
  case SYS_WRITE:
    esp += sizeof(int);
    int fd = *(int *)(esp);
    esp += sizeof(void *);
    void * buffer = (void *)(esp);
    esp += sizeof(size_t);
    size_t size = *(size_t *)(esp);
    putbuf(buffer, size);
    break;
  
  case SYS_EXIT:
    esp += sizeof(int);
    int status = *(int *)(esp);
    f->eax = status;
    thread_exit ();
    NOT_REACHED (); // as seen in thread.c, panic if thread cannot exit
    break;

  default:
    printf("Unknown system call\n");
    thread_exit ();
    NOT_REACHED ();
  }
}


