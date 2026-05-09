---
sidebar_position: 5
title: Phase 3 Audit Log
---

# Phase 3 Audit Log

## What and Why

Phase 3 records syscall activity in the kernel. Each audit entry stores process id, uid, syscall number, result, tick count, and command name. Admin users can read the audit ring with `audit_read()` and inspect it with `audit_dump`.

Medical-device systems need auditability because access decisions are not enough by themselves. Investigators also need to know what was attempted, what failed, and which identity was involved.

## Theory

The audit log is a fixed-size ring buffer. A ring is simple, bounded, and appropriate for xv6 because it avoids dynamic allocation in syscall paths. When the buffer fills, new events replace the oldest events.

The buffer is protected by a spinlock so concurrent harts cannot corrupt the head, tail, or entry data. `audit_read()` is admin-only because the log can reveal sensitive behavior.

## Implementation Walk-through

The kernel initializes the audit subsystem during boot. The syscall dispatcher records each syscall result after the handler returns.

Each entry includes:

```c
struct audit_entry {
  int pid;
  int uid;
  int syscall_no;
  int result;
  uint tick;
  char comm[16];
};
```

The trap handler also prints a human-readable audit line for syscall traps:

```text
[AUDIT] PID=<pid> UID=<uid> TRAP=Environment call (syscall) EPC=0x...
```

The admin tool `audit_dump` reads the ring and prints a table with syscall names and return values.

`audit_read()` returns the newest entries that fit the caller's buffer. That makes small readers useful even when the ring contains more events than their local buffer.

## How to Test

Log in as `patient1` and try to read audit data:

```sh
audit_dump
```

The request should be denied for non-admin users.

Log in as admin and run:

```sh
audit_dump
```

You should see syscall records. Then run:

```sh
compliance_test
```

Tests T13 through T17 verify admin-only audit reads, audit data availability, denied-open detection, successful-write detection, and attack detection.

## Common Pitfalls

The required trap audit print is very noisy because xv6 console output often writes one byte per syscall. When reviewing QEMU logs, strip lines that begin with `[AUDIT]` before reading compliance output.

Do not expose audit logs to normal users. A patient process should not be able to learn system activity by reading the ring.

Avoid unbounded audit storage in the kernel. A fixed-size ring keeps memory use predictable.
