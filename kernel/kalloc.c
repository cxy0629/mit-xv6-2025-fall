// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
  struct run *superfreelist; // 超级页空闲链表
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  // pa_end为PHYSTOP，值为0x88000000，本身已经和超级页对齐
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
#ifndef LAB_PGTBL
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
#else
  // 注：一定要预留够足够的超级页空间，否则会导致子进程在fork时复制失败
  int superpgs = 20; // 预留20个超级页的空间待分配
  char *super_p = (char *)((uint64)pa_end - superpgs * SUPERPGSIZE);
  // super_p已经和超级页和普通页对齐
  // 释放普通页
  for (; p + PGSIZE <= (char *)super_p; p += PGSIZE)
    kfree(p);
  // 释放超级页
  for (; super_p + SUPERPGSIZE <= (char *)pa_end; super_p += SUPERPGSIZE)
    superfree(super_p);
#endif
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}


// 释放超级页
#ifdef LAB_PGTBL
void superfree(void *pa)
{
  struct run *r;
  if(((uint64)pa % SUPERPGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("superfree");
  memset(pa, 1, SUPERPGSIZE);
  // 插入超级页空闲链表，使用空闲超级页自身的前8个字节存储链表指针
  r = (struct run *)pa;
  acquire(&kmem.lock);
  r->next = kmem.superfreelist;
  kmem.superfreelist = r;
  release(&kmem.lock);
}
#endif


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}


// 分配超级页
#ifdef LAB_PGTBL
void * superalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.superfreelist;
  if (r)
    kmem.superfreelist = r->next;
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, SUPERPGSIZE); // fill with junk
  return (void *)r;
}
#endif