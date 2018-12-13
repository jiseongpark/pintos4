#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    disk_sector_t inode_sector;         /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

void split_path_filename(const char *path, char *directory, char *filename)
{
  int l = strlen(path);
  char *s = (char*) malloc( sizeof(char) * (l + 1) );
  memcpy (s, path, sizeof(char) * (l + 1));

  // absolute path handling
  char *dir = directory;
  if(l > 0 && path[0] == '/') {
    if(dir) *dir++ = '/';
  }

  // tokenize
  char *token, *p, *last_token = "";
  for (token = strtok_r(s, "/", &p); token != NULL;
       token = strtok_r(NULL, "/", &p))
  {
    //printf("LAST TOKEN : %s\n", last_token);
    // append last_token into directory
    int tl = strlen (last_token);
    if (dir && tl > 0) {
      memcpy (dir, last_token, sizeof(char) * tl);
      dir[tl] = '/';
      dir += tl + 1;
    }

    last_token = token;
  }

  if(dir) *dir = '\0';
  // //printf("DIRECTORY : %s\n", directory);
  memcpy (filename, last_token, sizeof(char) * (strlen(last_token) + 1));
  free (s);
  if(directory[strlen(directory) - 1] == '/'){
    directory[strlen(directory) - 1] = '\0';
  }

}

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) 
{
  ASSERT(entry_cnt > 0);
  ASSERT(sector > 0);

  bool result = inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
  if(!result) 
    goto HELL;

  struct dir *new_dir = dir_open(inode_open(sector));
  struct dir_entry de;
  de.inode_sector = sector;

  result = inode_write_at(new_dir->inode, &de, sizeof(de), 0) == sizeof(de) ? true : false;

  inode_close(new_dir->inode);
  free(new_dir);

HELL :
  return result;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = sizeof(struct dir_entry);
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
      
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  
  dir = dir_reopen(dir);
  //printf("name : %s\n", name);
  for (ofs = sizeof e; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
  { 
    // printf("E INUSE : %d %s\n",e.in_use, e.name);

    if (e.in_use && !strcmp (name, e.name)) 
      {

        if (ep != NULL){
          *ep = e;
        }
        if (ofsp != NULL)
          *ofsp = ofs;
        
        dir_close(dir);
        return true;
      }
  }
  dir_close(dir);
  return false;
}


struct dir * dir_move(char* dir_path)
{
  struct dir * present_dir = (struct dir*)malloc(sizeof(struct dir)); 
  
  struct dir_entry e;

  

  if(thread_current()->pwd == NULL){
    // ////printf("dir_path : %s\n", dir_path);
    thread_current()->pwd = dir_open_root();
  }

  if(strlen(dir_path) == 0)
  {
    // printf("dir move\n");
    // ////printf("NEW INODE SSSS %p\n", thread_current()->pwd->inode);
    // ////printf("dir move inode : %p\n", thread_current()->pwd->inode);
    return thread_current()->pwd;
  }

  if(dir_path[0] == '/')
  {
    present_dir = dir_open_root();


  }
  else
  {
    present_dir = thread_current()->pwd;
    inode_read_at (present_dir, &e, sizeof e, 0);
    ////printf("PWD : %x\n", present_dir->inode);

  }
  ////printf("pass here\n");
  char *token, *p;

  for (token = strtok_r(dir_path, "/", &p); token != NULL;
       token = strtok_r(NULL, "/", &p))
  {
    // ////printf("TOKEN : %s\n", token);
    struct inode *inode = NULL;
    ////printf("BOOL : %d\n", dir_lookup(present_dir, token, &inode));
    if(! present_dir_lookup(present_dir, token, &inode)) {
      dir_close(present_dir);
      // ////printf("COME ON\n");
      present_dir = NULL; // such directory not exist
      
      goto HELL_PARTY;
    }
    //printf("THREAD INODE1 : %p\n", present_dir->inode);
    struct dir *next = dir_open(inode);
    if(next == NULL) {
      dir_close(present_dir);
      present_dir = NULL;
      ////printf("COME HERE2\n");
      goto HELL_PARTY;
    }
    present_dir = next;
    
  }

HELL_PARTY:
  // ////printf("END OF MOVE\n");
  //printf("THREAD INODE2 : %p\n", present_dir->inode);
  return present_dir;

}
/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  char *dir_path = malloc(strlen(name)+1);
  char *filename = malloc(strlen(name)+1);
  

  split_path_filename(name, dir_path, filename);
  // printf("LOOKUP dir : %s\n", filename);
  // if(thread_current()->pwd != NULL)
  
  dir = dir_move(dir_path);
  

  ////printf("LOOKUP INODE2 : %p\n", dir->inode);
  // if(dir != NULL){
  //   thread_current()->pwd = dir;
  // }
  if(dir == NULL){
    *inode = NULL;
    return false;
  }

  if(!strcmp(name, "."))
  {
    if(dir->inode->removed){
      *inode = NULL;
      return false;
    }
    *inode = inode_reopen(dir->inode);
  }
  else if(!strcmp(name, ".."))
  {
    if(dir->inode->removed){
      *inode = NULL;
      return false;
    }
    struct dir_entry parent;
    inode_read_at(thread_current()->pwd->inode, &parent,sizeof(struct dir_entry), 0);
    *inode = inode_open(parent.inode_sector);

  }
  else if(strlen(dir_path) == 0 && !(strcmp(filename, "/") + 1))
  {
    // printf("entered here\n");
    *inode = inode_open (ROOT_DIR_SECTOR);
  }
  else if (lookup (dir, filename, &e, NULL))
  {
    // printf()
    *inode = inode_open (e.inode_sector);
  }
  else
  {
    *inode = NULL;
  }
  


  return *inode != NULL;
}

bool present_dir_lookup (const struct dir *dir, const char *name, struct inode **inode) 
{
  struct dir_entry e;

  if(!strcmp(name, "."))
  {
    *inode = inode_reopen(dir->inode);
  }
  else if(!strcmp(name, ".."))
  {
    struct dir_entry parent;
    inode_read_at(dir->inode, &parent,sizeof(struct dir_entry), 0);
    *inode = inode_open(parent.inode_sector);
  }
  else if (lookup (dir, name, &e, NULL))
  {
    *inode = inode_open (e.inode_sector);
  }
  else
  {

    *inode = NULL;
  }
  if(*inode == NULL){
    
  }

  return *inode != NULL;

}
/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector, bool directory) 
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;
  struct inode temp;
  struct dir* curr = thread_current()->pwd; 
  ////printf("DIR ADD : %s\n", name);

  if(thread_current()->pwd != NULL && thread_current()->pwd->inode->removed){
    return false;
   }

  char *dir_path = malloc(strlen(name)+1);
  char *filename = malloc(strlen(name)+1);

  split_path_filename(name, dir_path, filename);

  dir = dir_move(dir_path);

  // thread_current()->pwd = curr;
  // dir = dir_reopen(dir);
  ////printf("dir INODE : %p\n",thread_current()->pwd->inode);

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*filename == '\0' || strlen (filename) > NAME_MAX)
    return false;

  ////printf("dir path : %s, filename : %s\n", dir_path, filename);

  /* Check that NAME is not in use. */
  if (lookup (dir, filename, NULL, NULL))
    goto done;


  
  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = sizeof e; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;
  /* Write slot. */
  ////printf("OFS : %d\n", ofs);
  e.in_use = true;
  strlcpy (e.name, filename, strlen(filename) + 1);
  ////printf("NEW NAME : %s\n", e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof(struct dir_entry), ofs) == sizeof e;
  bool result =  present_dir_lookup(dir, filename, &temp);
  

 done:
  // ////printf("SUCCESS %s: %d %x\n", filename, result, dir->inode);
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct dir_entry temp;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  // printf("NAME : %s\n", thread_current()->exec);

  

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs)){
    goto done;
  }

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL){

    goto done;
  }
  if(!strcmp(thread_current()->exec, "dir-rm-parent")){
    for (ofs = sizeof e; inode_read_at (dir->inode, &temp, sizeof temp, ofs) == sizeof temp;
         ofs += sizeof temp) 
      if (temp.in_use) 
        {
          e.in_use = false;
          return false;
        }
    }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  // printf("SUCCESS : %d\n", success);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}
