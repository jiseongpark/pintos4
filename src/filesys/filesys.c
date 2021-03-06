#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;
extern int dir_num;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");
  
  buffer_cache_init();
  inode_init ();
  free_map_init ();
  

  if (format){
    do_format ();

  }
  free_map_open ();

}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  buffer_cache_flush_all();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool directory) 
{
  
  
  disk_sector_t inode_sector = 0;

  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, directory)
                  && dir_add (dir, name, inode_sector, directory));

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
  struct dir *dir;
  
  // printf("filesys_open name : %s\n", name);
  
  if(thread_current()->pwd == NULL){
      dir = dir_open_root();
      // printf("filesys_open : pwd NULL\n");

    }
  else dir = thread_current()->pwd;
  struct inode *inode = NULL;

  
  if (dir != NULL)
    dir_lookup (dir, name, &inode);
  // dir_close (dir);
  // printf("CURRENT WORKING DIRECTORY INODE: %p\n", inode);
  
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir;
  char *dir_path = malloc(strlen(name)+1);
  char *filename = malloc(strlen(name)+1);
  split_path_filename(name, dir_path, filename);
  // printf("DIR PATH : %s\n", dir_path);


  if(strlen(dir_path) != 0){ 
    dir = dir_move(dir_path);
  }
  else{
    if(dir_num > 100)
      dir = thread_current()->pwd;
    else
      dir = dir_open_root();
    // dir = dir_open_root();

  }

  bool success = dir != NULL && dir_remove (dir, filename);
  if(dir != thread_current()->pwd)
    dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  // printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  // printf ("done.\n");
}
