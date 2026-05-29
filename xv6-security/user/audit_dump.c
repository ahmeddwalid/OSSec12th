#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/syscall.h"
#include "kernel/audit.h"
#include "user/user.h"

// maps syscall number to human-readable name. must stay in sync with syscall.h.
static char*
syscall_name(int num)
{
  switch(num){
  case SYS_fork: return "fork";
  case SYS_exit: return "exit";
  case SYS_wait: return "wait";
  case SYS_pipe: return "pipe";
  case SYS_read: return "read";
  case SYS_kill: return "kill";
  case SYS_exec: return "exec";
  case SYS_fstat: return "fstat";
  case SYS_chdir: return "chdir";
  case SYS_dup: return "dup";
  case SYS_getpid: return "getpid";
  case SYS_sbrk: return "sbrk";
  case SYS_pause: return "pause";
  case SYS_uptime: return "uptime";
  case SYS_open: return "open";
  case SYS_write: return "write";
  case SYS_mknod: return "mknod";
  case SYS_unlink: return "unlink";
  case SYS_link: return "link";
  case SYS_mkdir: return "mkdir";
  case SYS_close: return "close";
  case SYS_useradd: return "useradd";
  case SYS_userdel: return "userdel";
  case SYS_passwd: return "passwd";
  case SYS_whoami: return "whoami";
  case SYS_login: return "login";
  case SYS_chmod: return "chmod";
  case SYS_chown: return "chown";
  case SYS_audit_read: return "audit_read";
  default: return "unknown";
  }
}

int
main(void)
{
  struct audit_entry entries[96];
  int n = audit_read((char*)entries, sizeof(entries));  // raw kernel buffer dump
  int count;

  if(n < 0){
    printf("Permission denied.\n");  // non-admin gets EPERM
    exit(1);
  }
  count = n / sizeof(struct audit_entry);
  printf("tick pid uid syscall result comm\n");
  for(int i = 0; i < count; i++){
    printf("%d %d %d %s %d %s\n",
           entries[i].tick,
           entries[i].pid,
           entries[i].uid,
           syscall_name(entries[i].syscall_no),
           entries[i].result,
           entries[i].comm);
  }
  exit(0);
}