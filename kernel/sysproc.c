#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  // 测试panic内的backtrace功能
  // panic("sys_pause: test backtrace\n");
  backtrace();
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// sys_sigalarm的工作是注册alarm handler和触发间隔，并重置ticks计数器
// 当进程在用户态下运行时，ticks计数器在每次时钟中断时递增，到达触发间隔时执行handler函数并重置计数器
// 相关检查逻辑在trap.c的usertrap()中实现，此处只负责注册
uint64 sys_sigalarm(void)
{
  struct proc *p = myproc();
  int interval;
  uint64 handler;
  argint(0, &interval);
  argaddr(1, &handler);
  p->interval = interval;
  p->handler = (void (*)())handler;
  p->ticks = 0; // 重置计数
  p->in_alarm = 0;
  return 0;
}

uint64 sys_sigreturn(void)
{
  struct proc *p = myproc();
  // 恢复用户态现场
  // sigreturn也是一个系统调用，它的返回路径仍然经过useret，对于寄存器的恢复仍然是经由trapframe来完成的
  // 所以我们只需要将之前保存至alarm_trapframe的现场信息恢复到trapframe即可，就不需要写汇编代码来单独恢复寄存器了
  *(p->trapframe) = *(p->alarm_trapframe);
  p->in_alarm = 0; // 返回后不在alarm状态
  return 0;
}
