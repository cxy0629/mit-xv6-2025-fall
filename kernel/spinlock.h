// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
#ifdef LAB_LOCK
  int nts;
  int n;
#endif
};

#ifdef LAB_LOCK
// Reader-writer lock.
// 用内部spinlock保护读写锁的状态变量
// 再用这些状态变量形成读写规则
// 最终由这些规则保护外部共享数据
struct rwspinlock {
  // Replace this with your implementation.
  struct spinlock l;   // 保护读写者状态信息，供共享读写
  int readers;         // 正在临界区中的读者数量
  int writer;          // 临界区中是否有写者
  int waiting_writers; // 等待中的写者数量
};
#endif
