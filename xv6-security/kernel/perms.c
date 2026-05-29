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

// maps r/w/x + klass (0=owner,1=group,2=other) to the matching permission bit.
static int
access_bit(char access, int klass)
{
  if(klass == 0){
    if(access == 'r') return PERM_OWNER_READ;
    if(access == 'w') return PERM_OWNER_WRITE;
    if(access == 'x') return PERM_OWNER_EXEC;
  } else if(klass == 1){
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

// unix-style DAC: resolves caller to owner(0)/group(1)/other(2) and checks the
// matching mode bit. root (uid==0) always passes. resolution order matters:
// owner match takes priority over group match.
int
perm_check(struct inode *ip, char access)
{
  struct proc *p = myproc();
  int class;
  int bit;

  if(ip == 0)
    return 0;
  if(p == 0 || p->uid == 0)  // kernel threads and root bypass dac
    return 1;
  if(ip->uid == p->uid)
    class = 0;       // file owner
  else if(ip->gid == p->gid)
    class = 1;       // group member
  else
    class = 2;       // everyone else
  bit = access_bit(access, class);
  return bit != 0 && (ip->mode & bit) != 0;
}