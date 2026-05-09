---
sidebar_position: 5
title: Phase 3 Audit Log
---

# Phase 3: Audit Log

## Motivation

Authentication and file permissions control *what can happen*. But after a security incident, investigators need to know *what did happen*, *when*, and *who triggered it*. Without an audit trail:

- A clinician could claim they never wrote a dosage file.
- An intrusion could go undetected indefinitely.
- Regulatory review would have no evidence to examine.

Phase 3 adds a kernel-resident audit ring buffer that records every security-relevant syscall event.

## Design: Ring Buffer

A ring buffer is the right structure for kernel-space audit logging because:

1. **Bounded memory**: No dynamic allocation in syscall paths.
2. **O(1) write**: Head and tail pointer arithmetic only.
3. **Graceful overwrite**: Oldest events are silently dropped when full, keeping the system live.

```
Ring buffer: 256 slots, each is struct audit_entry

head ───▶ [entry 0] [entry 1] ... [entry N] ──▶ tail wraps
           oldest                              newest
```

The buffer is protected by a `spinlock_t` so concurrent harts cannot corrupt the shared head/tail pointers.

## `struct audit_entry`

```c
/* kernel/audit.h */
struct audit_entry {
  int  pid;           // process ID of caller
  int  uid;           // user ID of caller (from proc->uid)
  int  syscall_no;    // SYS_open, SYS_write, SYS_login, ...
  int  result;        // return value of the syscall
  uint tick;          // kernel tick count (for ordering)
  char comm[16];      // first 15 chars of p->name
};

#define AUDIT_RING_SIZE 256
```

## `audit_log()` Implementation

Called at the end of every syscall handler (before returning to user space):

```c
// kernel/audit.c
void audit_log(int syscall_no, int result) {
  struct proc *p = myproc();

  acquire(&audit_lock);
  struct audit_entry *e = &ring[tail % AUDIT_RING_SIZE];
  e->pid        = p->pid;
  e->uid        = p->uid;
  e->syscall_no = syscall_no;
  e->result     = result;
  e->tick       = ticks;
  memmove(e->comm, p->name, sizeof e->comm);
  tail++;
  if (tail - head > AUDIT_RING_SIZE) head++;  // overwrite oldest
  release(&audit_lock);
}
```

## Trap-Level Audit Printing

`kernel/trap.c` prints a one-line record for every syscall trap entry via `scause_name()`:

```
[TRAP] pid=3 uid=0 trap=Environment call (U-mode) epc=0x0000000080001234
```

This provides a real-time stream that QEMU captures in its terminal output, independent of the ring buffer. The `scause_name()` helper translates all standard RISC-V scause codes into human-readable strings.

:::tip
The trap audit lines are very noisy, one per syscall, and `printf` itself triggers write syscalls. During compliance testing, focus on the ring buffer output from `audit_dump`, not the raw trap lines.
:::

## `audit_read`: Admin-Only Access

```c
// kernel/sysproc.c
int sys_audit_read(void) {
  struct proc *p = myproc();
  if (p->uid != 0)              // not admin
    return -1;                  // EPERM

  // copy ring entries to user buffer ...
}
```

Non-admin callers receive `-1` immediately. The kernel does not reveal even the buffer size to unprivileged processes.

## `audit_dump` Tool

Inside xv6, the admin can run:

```sh
$ audit_dump
PID  UID  SYSCALL       RESULT  TICK  COMM
---  ---  -------       ------  ----  ----
3    0    SYS_login     0       12    login
4    0    SYS_open      3       45    sh
4    2    SYS_open      -1      67    compliance
4    0    SYS_audit_r   0       89    compliance
```

A `-1` result on `SYS_open` corresponds to a denied access, exactly the kind of evidence Phase 2 + Phase 3 together provide.

![QEMU terminal showing audit_dump table with ring buffer entries](/img/screenshots/phase3-audit-dump.png)

## Compliance Coverage

| Test | What it checks |
|------|---------------|
| T13 | `audit_read` by non-admin returns `-EPERM` |
| T14 | `audit_read` by admin succeeds and returns entries |
| T15 | A denied open (from T07) appears in the audit log |
| T16 | A successful write (from T10) appears in the audit log |
| T17 | Full end-to-end: login → denied open → audit confirms the denial |

## Security Notes

- Audit records are in-kernel only. A patient process cannot reach the buffer.
- The ring is **volatile**. It does not survive a reboot. A production system would flush to persistent storage.
- Audit should be paired with access control, not treated as a substitute for it. Logging a denial is useful; *preventing* the action is essential.

