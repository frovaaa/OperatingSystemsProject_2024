#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/shutdown.h"
#include "lib/kernel/hash.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);

typedef void (*handler) (struct intr_frame *);
static void syscall_exit (struct intr_frame *);
static void syscall_exec (struct intr_frame *);
static void syscall_wait (struct intr_frame *);
static void syscall_write (struct intr_frame *);
static void syscall_halt (struct intr_frame *f);
static void syscall_create (struct intr_frame *f);
static void syscall_remove (struct intr_frame *f);
static void syscall_filesize (struct intr_frame *f);
static void syscall_close (struct intr_frame *f);
static void syscall_tell (struct intr_frame *f);
static void syscall_seek (struct intr_frame *f);
static void syscall_read (struct intr_frame *f);
static void syscall_open (struct intr_frame *f);
static bool check_user_address (void *);

#define SYSCALL_MAX_CODE 19
static handler call[SYSCALL_MAX_CODE + 1];

unsigned item_hash (const struct hash_elem *e, void *aux);
bool item_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux);


struct item {
  int fd;
  struct file *file;
  struct hash_elem elem;
};

unsigned item_hash (const struct hash_elem *e, void *aux){
  const struct item *item = hash_entry(e, struct item, elem);
  return hash_int(item->fd);
}

bool item_compare (const struct hash_elem *a, const struct hash_elem *b, void *aux){
  const struct item *item_a = hash_entry(a, struct item, elem);
  const struct item *item_b = hash_entry(b, struct item, elem);
  return item_a->fd < item_b->fd;
}

struct hash file_table;
struct semaphore file_table_lock;

unsigned next_fd = 2;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  hash_init(&file_table, item_hash, item_compare, NULL);
  sema_init(&file_table_lock, 1);

  /* Any syscall not registered here should be NULL (0) in the call array. */
  memset(call, 0, SYSCALL_MAX_CODE + 1);

  /* Check file lib/syscall-nr.h for all the syscall codes and file
   * lib/user/syscall.c for a short explanation of each system call. */
  call[SYS_EXIT]  = syscall_exit;   /* Terminate this process. */
  call[SYS_EXEC]  = syscall_exec;   /* Start another process. */
  call[SYS_WAIT]  = syscall_wait;   /* Wait for a child process to die. */
  call[SYS_WRITE] = syscall_write;  /* Write to a file. */
  call[SYS_HALT] = syscall_halt;    // Halt the operating system.
  call[SYS_CREATE] = syscall_create; // Create a file
  call[SYS_REMOVE] = syscall_remove; // Remove a file
  call[SYS_FILESIZE] = syscall_filesize; // Get the size of a file
  call[SYS_CLOSE] = syscall_close; // Close a file
  call[SYS_TELL] = syscall_tell; // Get the position of a file
  call[SYS_SEEK] = syscall_seek; // Set the position of a file
  call[SYS_READ] = syscall_read; // Read from a file
  call[SYS_OPEN] = syscall_open; // Open a file
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
  thread_get_child_data(t->parent, t->tid)->exit_status = t->exit_status;
  thread_exit ();
}

static void
syscall_exec (struct intr_frame * f)
{
  int * stackpointer = f->esp;
  char * command = (char *) *(stackpointer + 1);

  if (check_user_address (command))
    f->eax = process_execute (command);
  else
    f->eax = -1;
}

static void
syscall_wait (struct intr_frame * f)
{
  int * stackpointer = (void *) f->esp;
  tid_t child_tid = *(stackpointer + 1);
  f->eax = process_wait (child_tid);
}

static void
syscall_write (struct intr_frame *f)
{
  int *stack = f->esp;
  ASSERT (*(stack+1) == 1); // fd 1 means stdout (standard output)
  char * buffer = *(stack+2);
  int    length = *(stack+3);
  putbuf (buffer, length);
  f->eax = length;
}


static void
syscall_halt (struct intr_frame *f)
{
  shutdown_power_off();
}

static void
syscall_create (struct intr_frame *f){
  // Lock the file table (we are doing file system operations so we lock)
  sema_down(&file_table_lock);

  // Get the file name and size from the stack
  int *stack = f->esp;
  const char *file = *(stack + 1);

  // Check if the pointer is valid
  if (check_user_address(file) == false){
    f->eax = -1;
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Get the initial size of the file
  // + 2 as the stack pointer is never incremented
  unsigned initial_size = *(stack + 2);

  // Create the file and get the result
  bool result = filesys_create(file, initial_size);

  // Put the result in the return value
  f->eax = result;

  // Unlock the file table
  sema_up(&file_table_lock);
}

static void
syscall_remove (struct intr_frame *f){
  // Lock the file table (we are doing file system operations so we lock)
  sema_down(&file_table_lock);

  // Get the file name from the stack
  int *stack = f->esp;
  const char *file = *(stack + 1);

  // Check if the pointer is valid
  if (check_user_address(file) == false){
    f->eax = -1;
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Remove the file and get the result
  bool result = filesys_remove(file);

  // Put the result in the return value
  f->eax = result;

  // Unlock the file table
  sema_up(&file_table_lock);
}

static void
syscall_filesize (struct intr_frame *f){
  // Lock the file table (we are doing file system operations so we lock)
  sema_down(&file_table_lock);

  // Get the file descriptor from the stack
  int *stack = f->esp;
  int fd = *(stack + 1);

  // Get the file from the file table
  struct item item;
  item.fd = fd;
  struct hash_elem *e = hash_find(&file_table, &item.elem);

  // Check if the file is in the file table
  if (e == NULL){
    f->eax = -1;
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Get the file from the hash element
  struct item *file = hash_entry(e, struct item, elem);

  // Get the file size
  int size = file_length(file->file);

  // Put the size in the return value
  f->eax = size;

  // Unlock the file table
  sema_up(&file_table_lock);
}

static void
syscall_close (struct intr_frame *f){
  // Lock the file table (we are doing file system operations so we lock)
  sema_down(&file_table_lock);

  // Get the file descriptor from the stack
  int *stack = f->esp;
  int fd = *(stack + 1);

  // Get the file from the file table
  struct item item;
  item.fd = fd;
  struct hash_elem *e = hash_find(&file_table, &item.elem);

  // Check if the file is in the file table
  if (e == NULL){
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Get the file from the hash element
  struct item *file = hash_entry(e, struct item, elem);

  // Close the file
  file_close(file->file);

  // Remove the file from the file table
  hash_delete(&file_table, &file->elem);

  // Unlock the file table
  sema_up(&file_table_lock);
}

static void
syscall_tell (struct intr_frame *f){
  // Lock the file table (we are doing file system operations so we lock)
  sema_down(&file_table_lock);

  // Get the file descriptor from the stack
  int *stack = f->esp;
  int fd = *(stack + 1);

  // Get the file from the file table
  struct item item;
  item.fd = fd;
  struct hash_elem *e = hash_find(&file_table, &item.elem);

  // Check if the file is in the file table
  if (e == NULL){
    f->eax = -1;
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Get the file from the hash element
  struct item *file = hash_entry(e, struct item, elem);

  // Get the file position
  int pos = file_tell(file->file);

  // Put the position in the return value
  f->eax = pos;

  // Unlock the file table
  sema_up(&file_table_lock);
}

static void
syscall_seek (struct intr_frame *f){
  // Lock the file table (we are doing file system operations so we lock)
  sema_down(&file_table_lock);

  // Get the file descriptor and position from the stack
  int *stack = f->esp;
  int fd = *(stack + 1);
  unsigned position = *(stack + 2);

  // Get the file from the file table
  struct item item;
  item.fd = fd;
  struct hash_elem *e = hash_find(&file_table, &item.elem);

  // Check if the file is in the file table
  if (e == NULL){
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Get the file from the hash element
  struct item *file = hash_entry(e, struct item, elem);

  // Seek the file
  file_seek(file->file, position);

  // Unlock the file table
  sema_up(&file_table_lock);
}

static void
syscall_read (struct intr_frame *f){
  // Lock the file table (we are doing file system operations so we lock)
  sema_down(&file_table_lock);

  // Get the file descriptor from the stack
  int *stack = f->esp;
  int fd = *(stack + 1);

  // Get the buffer and size from the stack
  char *buffer = *(stack + 2);
  unsigned size = *(stack + 3);

  // Check if the buffer is valid
  if (check_user_address(buffer) == false){
    f->eax = -1;
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Check if the file descriptor is 0 (stdin)
  // Fd 0 reads from the keyboard using input_getc()
  if (fd == 0){
    unsigned i;
    for (i = 0; i < size; i++){
      buffer[i] = input_getc();
    }
    f->eax = size;
    sema_up(&file_table_lock);
    return;
  }

  // Get the file from the file table
  struct item item;
  item.fd = fd;
  struct hash_elem *e = hash_find(&file_table, &item.elem);

  // Check if the file is in the file table
  if (e == NULL){
    f->eax = -1;
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Get the file from the hash element
  struct item *file = hash_entry(e, struct item, elem);

  // Read from the file
  int bytes_read = file_read(file->file, buffer, size);

  // Put the number of bytes read in the return value
  f->eax = bytes_read;

  // Unlock the file table
  sema_up(&file_table_lock);
}

static void
syscall_open (struct intr_frame *f){
  // Lock the file table (we are doing file system operations so we lock)
  sema_down(&file_table_lock);

  // Get the file name from the stack
  int *stack = f->esp;
  const char *file = *(stack + 1);

  // Check if the pointer is valid
  if (check_user_address(file) == false){
    f->eax = -1;
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Open the file
  struct file *file_opened = filesys_open(file);

  // Check if the file was opened
  if (file_opened == NULL){
    f->eax = -1;
    sema_up(&file_table_lock);
    syscall_exit(f);
  }

  // Create a new file item
  struct item *file_item = (struct item *)malloc(sizeof(struct item));
  file_item->fd = next_fd;
  file_item->file = file_opened;

  // Insert the file item in the file table
  hash_insert(&file_table, &file_item->elem);

  // Put the file descriptor in the return value
  f->eax = next_fd;

  // Increment the file descriptor
  next_fd++;

  // Unlock the file table
  sema_up(&file_table_lock);
}


static bool check_user_address (void * ptr) {
  return ptr != NULL && is_user_vaddr (ptr) && pagedir_get_page (thread_current ()->pagedir, ptr);
}
