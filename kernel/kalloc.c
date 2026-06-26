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
  // 引用计数数组，从end/PGSIZE开始有效
  int refcnt[PHYSTOP / PGSIZE];
} kmem;

void kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    // 将对应物理页计数设置为1，方便kfree进行计数减少管理空闲页面
    acquire(&kmem.lock);
    kmem.refcnt[(uint64)p / PGSIZE] = 1;
    release(&kmem.lock);
    kfree(p);
  }
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

  // 计数减少交给kfree处理，当计数减少至0时释放物理页，注意if-else两条路径的释放锁
  acquire(&kmem.lock);
  kmem.refcnt[(uint64)pa / PGSIZE]--;
  if (kmem.refcnt[(uint64)pa / PGSIZE] == 0)
  {
    release(&kmem.lock);
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run *)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  else
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  // 注意kalloc和kfree的不同点，即修改计数和释放/分配的顺序
  // kalloc必须在一个锁内完成分配和计数增加，
  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
  {
    kmem.freelist = r->next;
    kmem.refcnt[(uint64)r / PGSIZE]++;
  }
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk

  return (void *)r;
}

// 计数减少：kfree
// 计数增加：kalloc->新页，inc_refcnt->COW页
void inc_refcnt(void *pa)
{
  acquire(&kmem.lock);
  kmem.refcnt[(uint64)pa / PGSIZE]++;
  release(&kmem.lock);
}