#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/process.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

typedef void (*handler) (struct intr_frame *);
static void syscall_exit (struct intr_frame *f);
static void syscall_write (struct intr_frame *f);
static void syscall_wait (struct intr_frame *f);
static void syscall_exec (struct intr_frame *f);
static void syscall_halt (struct intr_frame *f);

#define SYSCALL_MAX_CODE 19
static handler call[SYSCALL_MAX_CODE + 1];

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

  /* Any syscall not registered here should be NULL (0) in the call array. */
  memset(call, 0, SYSCALL_MAX_CODE + 1);

  /* Check file lib/syscall-nr.h for all the syscall codes and file
   * lib/user/syscall.c for a short explanation of each system call. */
  call[SYS_EXIT]  = syscall_exit;   // Terminate this process.
  call[SYS_WRITE] = syscall_write;  // Write to a file.
  call[SYS_WAIT] = syscall_wait;    // wait for a child thread to finish
  call[SYS_EXEC] = syscall_exec;    // execute a new process
  call[SYS_HALT] = syscall_halt;    // Halt the operating system.
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_code = *((int*)f->esp);
  call[syscall_code](f);
}

static void
syscall_exit (struct intr_frame *f)
{
  int *stack = f->esp;
  struct thread* t = thread_current ();
  t->exit_status = *(stack+1);

  if (t->parent != NULL){
    struct child_elem * child_elem = thread_get_child(t->parent, t->tid);

    if (t->exit_status == -1){
      child_elem->cur_status = KILLED;
    } else {
      child_elem->cur_status = EXITED;
    }
  }

  
  thread_exit ();
}

static void
syscall_wait(struct intr_frame *f){
  int *stack = f->esp;
  // get the pid from the stack
  int pid = *(stack + 1);
  f->eax = process_wait(pid);
}

static void
syscall_write (struct intr_frame *f)
{
  int *stack = f->esp;
  ASSERT (*(stack+1) == 1); // fd 1
  char * buffer = *(stack+2);
  int    length = *(stack+3);
  putbuf (buffer, length);
  f->eax = length;
}

static void
syscall_exec (struct intr_frame *f){
  int *stack = f->esp;
  const char *cmd_line = *(stack + 1);

  // check if the pointer is valid

  if(!is_user_vaddr(cmd_line)){
    // push -1 on stack and call exit
    f->eax = -1;
    syscall_exit(f);
  }

  void * check = pagedir_get_page(thread_current()->pagedir, cmd_line);
  if(check == NULL){
    // push -1 on stack and call exit
    f->eax = -1;
    syscall_exit(f);
  }

  struct thread* parent = thread_current();
  tid_t pid = -1;

  pid = process_execute(cmd_line);

  struct child_elem * child_elem = thread_get_child(thread_get_by_tid(pid)->parent, pid);

  sema_down(&child_elem->child->child_load);

  if(!child_elem->successful_load){
    f->eax = -1;
  }

  f->eax = pid;
}

static void
syscall_halt (struct intr_frame *f)
{
  shutdown_power_off();
}
