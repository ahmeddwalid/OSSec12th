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

## Original xv6 vs Phase 3 Code Delta

Stock xv6 does not keep a security audit trail of syscall outcomes. This phase adds a dedicated audit subsystem and an admin-only read interface.

| Component | Stock xv6-riscv | Modified xv6-security |
|-----------|------------------|------------------------|
| Syscall event logging | No persistent syscall audit records | `audit_log(syscall_no, result)` called on every syscall return |
| Kernel storage | None | Fixed-size ring buffer (`AUDIT_BUF_SIZE = 256`) with spinlock protection |
| Audit record fields | N/A | `pid`, `uid`, `syscall_no`, `result`, `tick`, `comm` |
| User access path | N/A | `sys_audit_read` + `audit_dump` user tool |
| Access control | N/A | Audit read restricted to `uid == 0` |

Files touched for this phase: `kernel/audit.h`, `kernel/audit.c`, `kernel/syscall.c`, `kernel/sysfile.c`, `kernel/syscall.h`, and `user/audit_dump.c`.

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

#define AUDIT_BUF_SIZE 256
```

## `audit_log()` Implementation

Called at the end of every syscall handler (before returning to user space):

```c
// kernel/audit.c
void audit_log(int syscall_no, int result) {
  struct proc *p = myproc();

  acquire(&audit_lock);
  struct audit_entry *e = &ring[head];
  e->pid        = p->pid;
  e->uid        = p->uid;
  e->syscall_no = syscall_no;
  e->result     = result;
  e->tick       = ticks;
  memmove(e->comm, p->name, sizeof e->comm);
  head = (head + 1) % AUDIT_BUF_SIZE;
  if (count < AUDIT_BUF_SIZE)
    count++;
  else
    tail = (tail + 1) % AUDIT_BUF_SIZE;
  release(&audit_lock);
}
```

In this repository, the hook is placed in `kernel/syscall.c` after syscall dispatch and before returning to user mode, so both successful and denied operations are captured with the same mechanism.

## Trap-Level Audit Printing

`kernel/trap.c` uses `usertrap()` to intercept every syscall at the ECALL boundary. Security events are recorded into the ring buffer via `audit_log()` from the appropriate syscall handlers rather than printed directly from the trap handler. Printing from the trap handler would cause recursive spam, because `printf` itself invokes `write` syscalls, one character at a time. Each printed character would log another trap, which would print again.

The ring buffer approach is the correct mechanism: use `audit_dump` to inspect the captured entries after a session.

Relative to stock xv6, this separates auditing from console output and provides machine-readable records for compliance tests.

## `audit_read`: Admin-Only Access

```c
// kernel/sysfile.c
int sys_audit_read(void) {
  struct proc *p = myproc();
  if (p->uid != 0)              // not admin
    return -1;                  // EPERM

  // copy ring entries to user buffer ...
}
```

Non-admin callers receive `-1` immediately. The kernel does not reveal even the buffer size to unprivileged processes.

This is intentionally stricter than basic teaching-kernel behavior: audit observability is itself treated as privileged data.

## `audit_dump` Tool

Inside xv6, the admin can run:

```sh
$ audit_dump
tick pid uid syscall result comm
12 3 0 login 0 login
45 4 1 open 3 sh
67 4 1 open -1 compliance
89 4 0 audit_read 0 compliance
```

The columns are `tick pid uid syscall result comm`. The syscall name has no `SYS_` prefix. A `-1` result on `open` is a denied access. In the sample above, uid 1 (patient) tried to open a file and got -1. That is exactly the kind of evidence Phase 2 and Phase 3 together provide.

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

