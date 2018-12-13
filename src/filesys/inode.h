#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"
#include <list.h>


/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define DIRECT_MAPPING_SECTOR 123
#define INDIRECT_MAPPING_SECTOR 128

struct bitmap;

 /* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    disk_sector_t direct[DIRECT_MAPPING_SECTOR];   /* Direct mapping sector. */
    disk_sector_t indirect;                        /* Indirect mapping sector. */
    disk_sector_t dindirect;                       /* Doubly indirect mapping sector */

    bool directory;                                /* Check whether it is for directory */ 
    off_t length;                                  /* File size in bytes. */
    unsigned magic;                                /* Magic number. */
  };

struct inode_indirect
{
  disk_sector_t sec_num[INDIRECT_MAPPING_SECTOR];
};

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    struct inode *parent;               /* advance directory inode */
  };


void inode_init (void);
bool inode_create (disk_sector_t sector, off_t length, bool directory);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

#endif /* filesys/inode.h */
