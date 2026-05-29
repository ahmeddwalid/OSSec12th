#ifndef AUDIT_H
#define AUDIT_H

#define AUDIT_BUF_SIZE 256

// one entry per syscall invocation. packed as-is into the read buffer for user space.
struct audit_entry {
  int pid;
  int uid;
  int syscall_no;
  int result;        // 0=success, -1/permission-denied
  uint tick;         // kernel ticks since boot (uptime proxy)
  char comm[16];     // process name (truncated to 15 chars + nul)
};

void audit_init(void);
void audit_log(int syscall_no, int result);
int audit_read(char *buf, int bufsz);

#endif