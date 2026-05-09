#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "perms.h"

static int
access_bit(char access, int owner)
{
  if(owner == 0){
    if(access == 'r') return PERM_OWNER_READ;
    if(access == 'w') return PERM_OWNER_WRITE;
    if(access == 'x') return PERM_OWNER_EXEC;
  } else if(owner == 1){
    if(access == 'r') return PERM_GROUP_READ;
    if(access == 'w') return PERM_GROUP_WRITE;
    if(access == 'x') return PERM_GROUP_EXEC;
  } else {
    if(access == 'r') return PERM_OTHER_READ;
    if(access == 'w') return PERM_OTHER_WRITE;
    if(access == 'x') return PERM_OTHER_EXEC;
  }
  return 0;
}

int
perm_check(struct inode *ip, char access)
{
  struct proc *p = myproc();
  int class;
  int bit;

  if(ip == 0)
    return 0;
  if(p == 0 || p->uid == 0)
    return 1;
  if(ip->uid == p->uid)
    class = 0;
  else if(ip->gid == p->gid)
    class = 1;
  else
    class = 2;
  bit = access_bit(access, class);
  return bit != 0 && (ip->mode & bit) != 0;
}