---
slug: /
sidebar_position: 1
title: Project Overview
---

# Project Overview

## What and Why

This project turns xv6-riscv into a small teaching model for medical-device operating-system security. The original xv6 system boots directly into a shell and trusts every user process equally. That is helpful for learning kernel internals, but it is not acceptable for a device that stores patient records, dosage logs, or configuration data.

The project adds three security layers:

- Phase 1: authentication and account management.
- Phase 2: Unix-style file permissions on xv6 inodes.
- Phase 3: syscall audit logging and admin-only audit reads.

The bonus `compliance_test` program ties the phases together and produces an 18-test report.

## Theory

Medical-device security starts with basic operating-system isolation. A process should have an identity, files should carry ownership and permission metadata, and security-relevant actions should leave an audit trail. These ideas are common in production Unix systems, but xv6 is intentionally minimal, so each mechanism has to be wired into the kernel directly.

This implementation uses simple, inspectable mechanisms:

- A per-process `uid`, `gid`, role, username, and authenticated flag.
- File metadata fields for `mode`, `uid`, and `gid`.
- A fixed-size kernel audit ring buffer protected by a spinlock.

The goal is not to replace Linux security. The goal is to make each control visible enough for students to trace from syscall to kernel decision to user-visible behavior.

## Implementation Walk-through

Start with the boot path. `init` now launches `login`, and successful login executes `sh`. The kernel seeds `/etc/passwd` during filesystem initialization so the first boot has known demo accounts.

Then follow file access. `open`, `read`, `write`, and `exec` consult permission metadata through the permission helper. Admin users bypass permission checks, while patient and doctor identities are limited by owner, group, and other bits.

Finally, follow syscall return paths. Each syscall result is recorded in the audit ring. The admin-only `audit_read` syscall exposes the ring to `audit_dump` and `compliance_test`.

## How to Test

Build and boot xv6:

```bash
cd xv6-security
make clean
make
make qemu-nox
```

Log in as `admin` with password `admin123`, then run:

```sh
compliance_test
audit_dump
```

The compliance report should end with:

```text
Passed: 18 / 18
```

## Common Pitfalls

One easy mistake is treating xv6 like a normal Unix host. It has a tiny libc, a tiny shell, and no dynamic linker. Tests and tools must stay within the user library that xv6 provides.

Another pitfall is forgetting that audit printing is intentionally noisy. The trap audit line prints for every syscall trap, and xv6 writes console output one byte at a time. For verification, strip audit lines from captured QEMU output before reading test summaries.
