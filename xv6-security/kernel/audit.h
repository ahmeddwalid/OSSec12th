#ifndef AUDIT_H
#define AUDIT_H

#define AUDIT_BUF_SIZE 256

struct audit_entry {
  int pid;
  int uid;
  int syscall_no;
  int result;
  uint tick;
  char comm[16];
};

void audit_init(void);
void audit_log(int syscall_no, int result);
int audit_read(char *buf, int bufsz);

#endif