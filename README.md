# Project 3: File System

Group member:

11611712 杨寒梅

11611427 林森

## Task 1: Buffer cache

In this task, we are asked to implement a cache for file system, so everytime we write to a filesystem, we now write to the cache instead, the reading operation is the same, rather than read the block, we read from the cache. Since there is no file implements the cache function in the pintos, so I create two files, `cache.c` and `cache.h` to realize the function. 

### 1.1 Data structure and functions

According to the document, the cache size is 64, so we use `#define CACHE_SIZE 64` to define the size first.

We also define a struct named `cache_entry` to represent the cache.

```C
struct cache_entry {
  char data[BLOCK_SECTOR_SIZE];
  block_sector_t sector;

  bool dirty;                         /* dirty bit */
  bool access;                        /* reference bit */
  bool valid;                         /* valid or invalid entry */
};
```

Besides, we implement the following functions to complete the cache's function :

```C
void cache_init (void);
void cache_read (block_sector_t sector, void *target);
void cache_write (block_sector_t sector, void *source);
void cache_close (void);
struct cache_entry *cache_evict (void);
struct cache_entry *cache_find (block_sector_t sector);
```

As we said before, the read or write operation on the filesystem currently transfer to the cache, so we also need to change all the ` block_write` to `cache_write`, and all `block_read` to `cache_read` in the `inode.c`.

### 1.2 Algorithms

The core function in this task is `cache_read` and `cache_write`.  `cache_read` will first search cache entry from cache according to the sector number. If there is no such entry, we'll read the data from the block and find an empty entry to place it. The final data will copied to the `target`.

`cache_write` will first search cache entry from cache according to the sector number.  If there is no such entry, we'll read the data from the block and find an empty entry to place it. Finally, `source` will copied to the cache entry's data.

`cache_find` is a until function for  `cache_read` and `cache_write`, just iterate over the cache to find if hit or miss.

`cache_evict` is to find an empty (invalid) cache entry in the cache. If there isn't one, we use clock algorithm to evict a cache entry and make it invalid. Note that if the cache entry is dirty, we need write back.

`cache_close` is used when closing the cache. Each valid and dirty entry will be writen back.



### 1.3 Synchronization

We add a lock `cache_lock` to synchronize cache operations. Every time we have modification on the cache, we need to use `lock_acquire (&cache_lock);` to lock the operation, after we finish, we can use `lock_release (&cache_lock);` to release the lock. Operations like `cache_read`, `cache_write` and `cache_close` need to synchronize.

——————————————————感觉cache_evict 也需要！！！！！————————————————

### 1.4 Rationale

In this task, we implement a cache for filesystem. To achieve the goal, we create a new structure named `cache` and also realize some operations on cache. Lock is used in this task to prevent race condition.

## Task 2: Extensible files

### 2.1 Data structures and functions

### 2.2 Algorithms

### 2.3 Synchronization

### 2.4 Rationale



## Task 3: Subdirectories

### 3.1 Data structure and functions

### 3.2 Algorithms

### 3.3 Synchronization

### 3.4 Rationale



## Results

We pass all XXX tests.

此处应有截图

## Questions

**Q1: A reflection on the project–what exactly did each member do? What went well, and what could be improved?**

- Task1 :
  - Code : Hanmei Yang
  - Report : Hanmei Yang
- Task2:
  - Code : Sen Lin
  - Report : Sen Lin
- Task3:
  - Code: 
  - Report: 

What went well: We have a good division and cooperation. Every people focus on his/her job and if someone have problems, we discuss with each other and share our ideas. Due to the good teamwork, we finish this project quickly.

What could be improved: The time schedule for this project is really tight, we should start earier to make a more thorough design. When we finish task 1, it took many times to figure out the reason why it didn't work, that is we didn't implement the system write. Orginally we thought the thread creation is to blame and this misdirected idea affected the progress of this project.

**Q2: Does your code exhibit any major memory safety problems (especially regarding C strings), memory leaks, poor error handling, or race conditions?** 



**Q3: Did you use consistent code style? Your code should blend in with the existing Pintos code. Check your use of indentation, your spacing, and your naming conventions.** 

Yes. Our codes are consistent with the style of existing Pintos code. Take the methods' definition for example, the return type and other keywords are in one line, and the method name and parameters are in the next line. We leave each left parentheses `(`  a space between the before character. And each left brace `{` occupies one line. In terms of naming conventions, we notice that the functions and variables in pintos both use underline-naming principle, so we also use this kind of convention.

**Q4: Is your code simple and easy to understand?** 



**Q5: If you have very complex sections of code in your solution, did you add enough comments to explain them?** 



**Q6: Did you leave commented-out code in your final submission?** 



**Q7: Did you copy-paste code instead of creating reusable functions?** 



**Q8: Are your lines of source code excessively long? (more than 100 characters)** 



**Q9: Did you re-implement linked list algorithms instead of using the provided list manipu-**
**lation**



