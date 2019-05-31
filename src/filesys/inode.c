#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

static bool inode_allocate(struct inode_disk *disk_inode);

static bool inode_deallocate(struct inode *inode);

static bool inode_keep(struct inode_disk *disk_inode, off_t length);

static bool inode_keep_indirect(block_sector_t *p_entry, size_t num_sectors, int level);

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors(off_t size)
{
  return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

static inline size_t
minest(size_t a, size_t b)
{
  if(a < b)
  {
    return a;
  }
  else{
    return b;
  }
  
}

static block_sector_t
sector_to_index(const struct inode_disk *idisk, off_t index)
{
  off_t base = 0;

  off_t limit = 0; 

  block_sector_t sector_setter;

  
  limit = limit +  DIRECT_BLOCKS_COUNT ;

  if (index < limit)
  {
    return idisk->direct_blocks[index];
  }

  base = limit;



  limit = limit +  INDIRECT_BLOCKS_PER_SECTOR;
  if (index < limit)
  {
    struct inode_indirect_block_sector *indirect_idisk;

    indirect_idisk = calloc(1, sizeof(struct inode_indirect_block_sector));

    cache_read(idisk->indirect_block, indirect_idisk);

    sector_setter = indirect_idisk->blocks[index - base];

    free(indirect_idisk);

    return sector_setter;
  }

  base = limit;
  

  limit = limit + INDIRECT_BLOCKS_PER_SECTOR * INDIRECT_BLOCKS_PER_SECTOR;
  if (index < limit)
  {
 
    off_t first = (index - base) / INDIRECT_BLOCKS_PER_SECTOR;
    off_t second = (index - base) % INDIRECT_BLOCKS_PER_SECTOR;

   
    struct inode_indirect_block_sector *indirect_idisk;
    indirect_idisk = calloc(1, sizeof(struct inode_indirect_block_sector));

    cache_read(idisk->doubly_indirect_block, indirect_idisk);
    cache_read(indirect_idisk->blocks[first], indirect_idisk);
    sector_setter = indirect_idisk->blocks[second];

    free(indirect_idisk);
    return sector_setter;
  }

 
  return -1;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector(const struct inode *inode, off_t pos)
{
  ASSERT(inode != NULL);
  //   if (pos < inode->data.length)
  //     return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  if (0 <= pos && pos < inode->data.length)
  {
    // sector index
    off_t index = pos / BLOCK_SECTOR_SIZE;
    return sector_to_index(&inode->data, index);
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void inode_init(void)
{
  list_init(&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT(length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc(1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    //size_t sectors = bytes_to_sectors (length);
    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;
    disk_inode->is_dir = is_dir;
    if (inode_allocate(disk_inode))
    {
      /* Our implementation: cache write */
      cache_write(sector, disk_inode);
      // block_write (fs_device, sector, disk_inode);
      //           if (sectors > 0)
      //             {
      //               static char zeros[BLOCK_SECTOR_SIZE];
      //               size_t i;

      //               for (i = 0; i < sectors; i++)
      //                 /* Our implementation: cache write */
      //                 cache_write (disk_inode->start + i, zeros);
      //                 // block_write (fs_device, disk_inode->start + i, zeros);
      //             }
      success = true;
    }
    free(disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
       e = list_next(e))
  {
    inode = list_entry(e, struct inode, elem);
    if (inode->sector == sector)
    {
      inode_reopen(inode);
      return inode;
    }
  }

  /* Allocate memory. */
  inode = malloc(sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front(&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  /* Our implementation: cache read */
  cache_read(inode->sector, &inode->data);
  // block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    /* Remove from inode list and release lock. */
    list_remove(&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      free_map_release(inode->sector, 1);
      //           free_map_release (inode->data.start,
      //                             bytes_to_sectors (inode->data.length));
      inode_deallocate(inode);
    }

    free(inode);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void inode_remove(struct inode *inode)
{
  ASSERT(inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
  {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      /* Read full sector directly into caller's buffer. */
      /* Our implementation: cache read */
      cache_read(sector_idx, buffer + bytes_read);
      // block_read (fs_device, sector_idx, buffer + bytes_read);
    }
    else
    {
      /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
      if (bounce == NULL)
      {
        bounce = malloc(BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
          break;
      }
      /* Our implementation: cache read */
      cache_read(sector_idx, bounce);
      // block_read (fs_device, sector_idx, bounce);
      memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }
  free(bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size,
                     off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (byte_to_sector(inode, offset + size - 1) == -1u)
  {

    bool success;
    success = inode_keep(&inode->data, offset + size);
    if (!success)
      return 0; 

    inode->data.length = offset + size;
    cache_write(inode->sector, &inode->data);
  }

  while (size > 0)
  {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      /* Write full sector directly to disk. */
      // block_write (fs_device, sector_idx, buffer + bytes_written);
      /* Our implementation: cache write */
      cache_write(sector_idx, buffer + bytes_written);
    }
    else
    {
      /* We need a bounce buffer. */
      if (bounce == NULL)
      {
        bounce = malloc(BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
          break;
      }

      /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
        // block_read (fs_device, sector_idx, bounce);
        /* Our implementation: cache read */
        cache_read(sector_idx, bounce);
      else
        memset(bounce, 0, BLOCK_SECTOR_SIZE);
      memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
      // block_write (fs_device, sector_idx, bounce);
      /* Our implementation: cache write */
      cache_write(sector_idx, bounce);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }
  free(bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void inode_deny_write(struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write(struct inode *inode)
{
  ASSERT(inode->deny_write_cnt > 0);
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode)
{
  return inode->data.length;
}

static bool inode_allocate(struct inode_disk *disk_inode)
{
  return inode_keep(disk_inode, disk_inode->length);
}

static bool
inode_keep_indirect(block_sector_t *p_entry, size_t num_sectors, int level)
{
  static char zeros[BLOCK_SECTOR_SIZE];

  ASSERT(level <= 2);

  if (level == 0)
  {

    if (*p_entry == 0)
    {
      /* To pass dir-vine-persistence */
      if(!free_map_allocate(1, p_entry))
        return false;
      cache_write(*p_entry, zeros);
    }
    return true;
  }

  struct inode_indirect_block_sector indirect_block;
  if (*p_entry == 0)
  {
    /* To pass dir-vine-persistence */
    if(!free_map_allocate(1, p_entry))
      return false;
    cache_write(*p_entry, zeros);

  }
  
  cache_read(*p_entry, &indirect_block);

  size_t unit = (level == 1 ? 1 : INDIRECT_BLOCKS_PER_SECTOR);
  size_t i, l = DIV_ROUND_UP(num_sectors, unit);

  for (i = 0; i < l; ++i)
  {
    size_t subsize = minest(num_sectors, unit);

    if(!inode_keep_indirect(&indirect_block.blocks[i], subsize, level - 1))
      return false;

    num_sectors -= subsize;
  }

  ASSERT(num_sectors == 0);
  cache_write(*p_entry, &indirect_block);
  return true;
}

static bool
inode_keep(struct inode_disk *disk_inode, off_t length)
{
  static char zeros[BLOCK_SECTOR_SIZE];


  if (length < 0)
    return false;


  size_t num_sectors = bytes_to_sectors(length);
  size_t i, l;


  l = minest(num_sectors, DIRECT_BLOCKS_COUNT * 1);
  for (i = 0; i < l; ++i)
  {

    if (disk_inode->direct_blocks[i] == 0)
    { 
      /* To pass dir-vine-persistence */
      if(!free_map_allocate(1, &disk_inode->direct_blocks[i]))
        return false;
      cache_write(disk_inode->direct_blocks[i], zeros);
    }
  }
  num_sectors = num_sectors - l;
  if (num_sectors == 0)
    return true;


  l = minest(num_sectors, 1 * INDIRECT_BLOCKS_PER_SECTOR);
  if(!inode_keep_indirect(&disk_inode->indirect_block, l, 1))
    return false;
  num_sectors = num_sectors - l;
  if (num_sectors == 0)
    return true;

  l = minest(num_sectors, 1 * INDIRECT_BLOCKS_PER_SECTOR * INDIRECT_BLOCKS_PER_SECTOR);
  if(!inode_keep_indirect(&disk_inode->doubly_indirect_block, l, 2))
    return false;

  num_sectors = num_sectors - l;
  if (num_sectors == 0)
    return true;

  ASSERT(num_sectors == 0);
  return false;
}

static void
inode_de_indirect(block_sector_t entry, size_t num_sectors, int level)
{
  
  ASSERT(level <= 2);

  if (level == 0)
  {
    free_map_release(entry, 1);
    return;
  }

  struct inode_indirect_block_sector indirect_block;
  cache_read(entry, &indirect_block);

  size_t unit = (level == 1 ? 1 : INDIRECT_BLOCKS_PER_SECTOR);
  size_t i, l = DIV_ROUND_UP(num_sectors, unit);

  for (i = 0; i < l; ++i)
  {
    size_t subsize = minest(num_sectors, unit);
    inode_de_indirect(indirect_block.blocks[i], subsize, level - 1);
    num_sectors = num_sectors - subsize;
  }

  ASSERT(num_sectors == 0);
  free_map_release(entry, 1);
}

static bool inode_deallocate(struct inode *inode)
{
  off_t file_length = inode->data.length; 
  if (file_length < 0)
    return false;

 
  size_t num_sectors = bytes_to_sectors(file_length);
  size_t i, l;

  
  l = minest(num_sectors, DIRECT_BLOCKS_COUNT * 1);
  for (i = 0; i < l; ++i)
  {
    free_map_release(inode->data.direct_blocks[i], 1);
  }
  num_sectors = num_sectors - l;


  l = minest(num_sectors, 1 * INDIRECT_BLOCKS_PER_SECTOR);

  if (l > 0)
  {
    inode_de_indirect(inode->data.indirect_block, l, 1);
    num_sectors = num_sectors - l;
  }

 
  l = minest(num_sectors, 1 * INDIRECT_BLOCKS_PER_SECTOR * INDIRECT_BLOCKS_PER_SECTOR);
  if (l > 0)
  {
    inode_de_indirect(inode->data.doubly_indirect_block, l, 2);
    num_sectors = num_sectors - l;
  }

  ASSERT(num_sectors == 0);
  return true;
}
