#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "userprog/exception.h"
#include "vm/page.h"
#include <string.h>

// int num = 0;
static void syscall_handler (struct intr_frame *);


void syscall_init (void) 
{
  list_init(&openfile_list);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED) 
{

  int *p = f->esp;
  int i = 0;
  thread_current()->esp = p;
  
  // printf("SYSCALL : %x\n", p);
  
  if(pagedir_get_page(thread_current()->pagedir, f->esp)==NULL){
     // printf("11111111111111111111\n");
     syscall_exit(-1);
  }
  
  switch(*p)
  {
     case SYS_HALT: /* 0 */
     syscall_halt();
     break;

     case SYS_EXIT: /* 1 */
     while(thread_current()->parent->status == THREAD_BLOCKED
     		&& thread_current()->parent->child_num != 1
     		&& thread_current()->tid != 3)
     {
     	//timer_sleep(100);
        thread_yield();
     }
     if(!is_user_vaddr(*(p+1))){
        syscall_exit(-1);
     }
     syscall_exit(*(p+1));
     break;
     
     case SYS_EXEC: /* 2 */
     if(pagedir_get_page(thread_current()->pagedir, *(p+1))==NULL)
     {
        f->eax=0;
        // printf("444444444444444444444\n");
        syscall_exit(-1);
        break;
     }
     f->eax = syscall_exec(*(p+1));
     break;
     case SYS_WAIT: /* 3 */
     // printf("2222222222222222222\n");
     // printf("P : %x\n", *(p+1));
     f->eax = syscall_wait(*(p+1));
     
     break;
     
     case SYS_CREATE: /* 4 */
     if(pagedir_get_page(thread_current()->pagedir, *(p+1))==NULL)
     {
        f->eax=0;
        // printf("555555555555555555\n");
        syscall_exit(-1);
        break;
     }

     f->eax = syscall_create(*(p+1), *(p+2));
     break;
     
     case SYS_REMOVE: /* 5 */
     f->eax = syscall_remove(*(p+1));
     break;
     
     case SYS_OPEN:  /* 6 */
     if(!is_user_vaddr(*(p+1))){
        syscall_exit(-1);
     }
     if(pagedir_get_page(thread_current()->pagedir, *(p+1))==NULL)
     {
        f->eax=0;
        syscall_exit(-1);
        break;
     }
     f->eax = syscall_open(*(p+1));
     // printf("!!!!!%d\n",f->eax);
     break;
     
     case SYS_FILESIZE:  /* 7 */
     f->eax = syscall_filesize(*(p+1));
     break;
     
     case SYS_READ:   /* 8 */
     // printf("BAD READ : %d\n", thread_current()->pagedir);
     if(!is_user_vaddr(*(p+2))){
        // printf("8888888888\n");
        syscall_exit(-1);
     }

   if(pagedir_get_page(thread_current()->pagedir, *(p+2))==NULL)
     {
        // printf("P + 2 :%x\n", *(p+2));
        // printf("ALIGN : %x\n", pg_round_down(*(p+2)));
        // printf("STACK END : %x\n", thread_current()->stack_end);
        if(PHYS_BASE-STACK_MAX <= *(p+2) 
            && PHYS_BASE > *(p+2) 
            && (thread_current()->esp <= *(p+2) 
              || *(p+2) == f->esp - 4 
              || *(p+2) == f->esp - 32))
        {
          stack_growth(*(p+2));
          stack_growth(*(p+2)+PGSIZE);
          // *(p+2) = pg_round_down(*(p+2));
          // printf("aligned p+2 : %x\n", *(p+2));
          goto N;  
        }
        
        // if(growth_condition(*(p+2), thread_current()->esp, f->esp))
        // {
        //   stack_growth(*(p+2));
        //   break;
        // }
        // goto N;
        

        f->eax=0;
        // printf("999999999\n");
        syscall_exit(-1);
        // break;
     }     
     N:  
     f->eax = syscall_read(*(p+1),*(p+2), *(p+3));
     break;
     
     case SYS_WRITE:   /* 9 */
     if(pagedir_get_page(thread_current()->pagedir, *(p+7))==NULL)
     {
        f->eax=0;
        // printf("aaaaaaaaaaaaa\n");
        syscall_exit(-1);
        break;
     }
     f->eax = syscall_write(*(p+6), *(p+7), *(p+8));
     
     break;
     
     case SYS_SEEK:   /* A */
     syscall_seek(*(p+1), *(p+2));
     break;
     
     case SYS_TELL:   /* B */
     f->eax = syscall_tell(*(p+1));
     break;
     
     case SYS_CLOSE:  /* C */
     syscall_close(*(p+1));
     break;
     
     default:
     // printf("ERROR at syscall_handler\n");
     break;
  }


}

void syscall_halt(void)
{
   power_off();
}

void syscall_exit(int status)
{

   struct thread * parent = thread_current()->parent;
   printf("%s: ", thread_current()->name);
   printf("exit(%d)\n", status);
   // printf("exit tid : %d\n", thread_current()->tid);

   thread_current()->exit_status = status;

   list_remove(&thread_current()->elem);
   thread_current()->parent->child_num -= 1;
   
   process_exit();
}

int syscall_open(const char * file)
{

   int fd;
   struct file_info *new_file = malloc(sizeof(struct file_info));
   
   if(file == NULL){
      free(new_file);
      return -1;
   }
   


   new_file->file = filesys_open(file);

   // file_deny_write(new_file->file);
   
   if(new_file->file == NULL){
      free(new_file);
      return -1;
   }
   else{
      
      fd = list_size(&openfile_list) + 3;
      // printf("fd : %d\n", fd);
      
      new_file->fd = fd;
      new_file->opener = thread_current()->tid;
      if(!strcmp(thread_current()->exec, file)){
         new_file->deny_flag = 1;
      }
      
      list_push_back(&openfile_list, &new_file->elem);
      // printf("list size : %d\n", list_size(&openfile_list));
      // printf("OPEN : %d\n", fd);
      return fd;
   }
}

bool syscall_create(const char *file, unsigned initial_size)
{
   if(file==NULL){
      syscall_exit(-1);
   }
   return filesys_create(file, initial_size);
}

pid_t syscall_exec(const char *cmd_line)
{
   int tid = 0;
   tid = process_execute(cmd_line);
   if(thread_current()->executable == 1){
      return -1;
   }
  return tid;   
}

int syscall_wait(pid_t pid)
{
   return process_wait(pid);
}

bool syscall_remove(const char *file)
{
   return filesys_remove(file);
}

int syscall_filesize(int fd)
{
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   for(; e != list_end(&openfile_list); e = list_next(e)){
      of = list_entry(e, struct file_info, elem);

      if(of->fd == fd)
      {
         return file_length(of->file);
      }
   }
}

int syscall_read(int fd, void *buffer, unsigned size)
{
   
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   int ret_size = 0;

   // printf("READ THREAD TID : %d\n", thread_current()->tid);

   switch(fd){
      case STDIN:
      return input_getc();
      
      case STDOUT:
      // printf("ccccccccccc\n");
      syscall_exit(-1);
      return -1;

      case STDERR:
      printf("syscall.c : syscall_read(STDERR, ...) : standard error entered\n");
      return -1;

      default:

      for(; e != list_end(&openfile_list); e = list_next(e)){
         of = list_entry(e, struct file_info, elem);
         if(of->fd == fd)
         {
            // sema_down(&of->sema);
            // of->read_count++;
            // if(of->read_count ==1)
            //    sema_down(&of->rw_sema);
            // sema_up(&of->sema);
            // printf("READ SIZE : %x\n", file_length(of->file));
            ret_size = file_read(of->file, buffer, size);
            
            // sema_down(&of->sema);
            // of->read_count--;
            // if(of->read_count == 0)
            //    sema_up(&of->rw_sema);
            // sema_up(&of->sema);

            return ret_size;
         }
      }
      return -1;
   }
}

int syscall_write(int fd, const void *buffer, unsigned size)
{
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   
   switch(fd){
      case STDIN:
      // printf("dddddddddddddddd\n");
      syscall_exit(-1);
      return -1;

      case STDOUT:
      putbuf(buffer, size);
      return size;

      case STDERR:
      printf("syscall.c : syscall_write(STDERR, ...) : standard error entered\n");
      return -1;

      default:
      // printf("WRITE : %d\n", fd);
      for(; e != list_end(&openfile_list); e = list_next(e)){
         of = list_entry(e, struct file_info, elem);
         if(of->fd == fd)
         {
            if(of->deny_flag == 1){
               return 0;
            } 
            // sema_down(&of->rw_sema);
            file_write(of->file, buffer, size);
            // sema_up(&of->rw_sema);
            return size;
         }
      }
      return -1;
   }
}

void syscall_seek(int fd, unsigned position)
{
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   
   for(; e != list_end(&openfile_list); e = list_next(e)){
      of = list_entry(e, struct file_info, elem);
      if(of->fd == fd)
      {
         file_seek(of->file, position);
         break;
      }
   }
}

unsigned syscall_tell(int fd)
{
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   
   for(; e != list_end(&openfile_list); e = list_next(e)){
      of = list_entry(e, struct file_info, elem);
      if(of->fd == fd)
      {
         return file_tell(of->file);
      }
   }
}

void syscall_close(int fd)
{
   // printf("CLOSE THREAD TID : %d\n", thread_current()->tid);
   struct file_info *of = NULL;
   struct list_elem *e = list_begin(&openfile_list);
   int flag = 0;
   // printf("CLOSE FD : %d\n", fd);
   for(; e != list_end(&openfile_list); e = list_next(e)){
      of = list_entry(e, struct file_info, elem);
      if(of->fd == fd)
      {
         flag = 1;
         break;
      }
   }
   if(!flag 
      || fd == 0 
      || fd == 1 
      || of->opener != thread_current()->tid)
   {
      syscall_exit(-1);
   }
   list_remove(e);
   file_close(of->file);
   
   free(of);
}