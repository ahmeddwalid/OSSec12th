#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "audit.h"

// 256-entry circular buffer. head is write position, tail is oldest valid entry.
// count tracks how many entries are live (clamped at AUDIT_BUF_SIZE).
static struct audit_entry ring[AUDIT_BUF_SIZE];
static int head;
static int tail;
static int count;
static struct spinlock audit_lock;

// called from main() early — before any process or syscall exists.
void
audit_init(void)
{
  initlock(&audit_lock, "audit");
  memset(ring, 0, sizeof(ring));
  head = 0;
  tail = 0;
  count = 0;
}

// called from syscall() on every invocation — success or failure. spinlock protects
// against concurrent writes from multiple cores. wraps around when full.
void
audit_log(int syscall_no, int result)
{
  struct proc *p = myproc();
  struct audit_entry *e;

  acquire(&audit_lock);
  e = &ring[head];
  e->pid = p ? p->pid : 0;
  e->uid = p ? p->uid : -1;
  e->syscall_no = syscall_no;
  e->result = result;
  e->tick = ticks;
  if(p)
    safestrcpy(e->comm, p->name, sizeof(e->comm));
  else
    e->comm[0] = 0;
  head = (head + 1) % AUDIT_BUF_SIZE;
  if(count < AUDIT_BUF_SIZE)
    count++;
  else
    tail = (tail + 1) % AUDIT_BUF_SIZE;
  release(&audit_lock);
}

// admin-only (uid==0). copies the oldest nentry entries into caller buffer.
// returns bytes written, -1 on permission denied.
int
audit_read(char *buf, int bufsz)
{
  struct proc *p = myproc();
  int copied = 0;
  int idx;

  if(p == 0 || p->uid != 0)
    return -1;
  acquire(&audit_lock);
  int capacity = bufsz / sizeof(struct audit_entry);
  int nentry = count;
  if(nentry > capacity)
    nentry = capacity;
  idx = (head - nentry + AUDIT_BUF_SIZE) % AUDIT_BUF_SIZE;
  for(int i = 0; i < nentry; i++){
    memmove(buf + copied, &ring[idx], sizeof(struct audit_entry));
    copied += sizeof(struct audit_entry);
    idx = (idx + 1) % AUDIT_BUF_SIZE;
  }
  release(&audit_lock);
  return copied;
}