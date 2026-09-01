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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  int nfree;  // 计数空闲页，减少遍历链表时的锁使用
} kmem[NCPU]; // 每一个CPU都有独立的freelist

static char kmem_lock_names[NCPU][8]; // 锁名称

void
kinit()
{
  // 初始化锁
  for(int i = 0; i < NCPU; i++){
    snprintf(kmem_lock_names[i], sizeof(kmem_lock_names[i]), "kmem%d", i);
    initlock(&kmem[i].lock, kmem_lock_names[i]);
  }

  freerange(end, (void*)PHYSTOP);
}

// freerange()在kinit()内被调用，kinit()在启动阶段由某一个CPU执行
// 因此freerange不需要显式传递一个CPU编号，在kfree()内根据当前CPU编号获取空闲页即可
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off(); // 保护读取cpuid

  int id = cpuid(); // 获取CPU编号，将所有空闲页分配给一个CPU（启动执行kinit()的CPU）

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  kmem[id].nfree += 1; // 增加空闲页计数
  release(&kmem[id].lock);

  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();

  int id = cpuid();

  // 当前CPU的freelist内有空闲页
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r){
    kmem[id].freelist = r->next;
    kmem[id].nfree--;
  }
  release(&kmem[id].lock);

  // 当前CPU的freelist内没有空闲页
  // 扫描其他CPU的freelist情况，一次性至多偷取4页
  if(r == 0){
    for(int i = 0; i < NCPU; i++){
      // 跳过当前CPU
      if(i == id) continue;

      struct run *head = 0;
      struct run *tail = 0;

      acquire(&kmem[i].lock);

      int steal = kmem[i].nfree;
      if(steal > 4) steal = 4;

      if(steal > 0){
        head = kmem[i].freelist;
        tail = kmem[i].freelist;

        // 偷取freelist前半部分
        for(int j = 1; j < steal; j++)
          tail = tail->next;
        
        kmem[i].freelist = tail->next;

        // 构建新的freelist：from head to tail
        tail->next = 0;
      }

      kmem[i].nfree -= steal;

      release(&kmem[i].lock);

      // 偷取成功
      if(head){
        // 获取第一个空闲页用于kalloc，并从freelist中移除
        r = head;
        head = head->next;

        // 若还有空闲页，则挂载至当前CPU的freelist上
        if(head){
          acquire(&kmem[id].lock);
          kmem[id].freelist = head;
          kmem[id].nfree += steal - 1;
          release(&kmem[id].lock);
        }
        // 偷取结束
        break;
      }
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
