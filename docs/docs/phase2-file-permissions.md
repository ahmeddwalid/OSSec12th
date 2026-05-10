---
sidebar_position: 4
title: Phase 2 File Permissions
---

# Phase 2: File Permissions

## Motivation

After Phase 1 establishes *who* the user is, Phase 2 answers *what* they are allowed to touch. Without file permissions, a logged-in patient can open `/device/config`, overwrite `/dosage/insulin.log`, or read the audit log: defeating the entire point of authentication.

Phase 2 adds Unix-style DAC (Discretionary Access Control) to every inode.

## Original xv6 vs Phase 2 Code Delta

Stock xv6 has inode ownership fields in memory but not full Unix-style per-inode permission enforcement across all relevant syscall paths. This phase makes permissions first-class and enforced end-to-end.

| Component | Stock xv6-riscv | Modified xv6-security |
|-----------|------------------|------------------------|
| On-disk inode metadata | No `mode/uid/gid` persisted in this project baseline | Added `mode`, `uid`, `gid` to `struct dinode` |
| In-memory inode/stat propagation | Limited metadata for DAC decisions | `mode`, `uid`, `gid` loaded/stored via `ilock`, `iupdate`, `stati` |
| Permission helper | None | `perm_check(ip, access)` in `kernel/perms.c` |
| Enforcement points | Mostly open-time semantics | Explicit checks in `sys_open`, `fileread`, `filewrite`, `sys_exec` |
| File ownership initialization | Generic file creation defaults | Ownership defaults set from caller identity during inode allocation |

Files touched for this phase: `kernel/fs.h`, `kernel/file.h`, `kernel/stat.h`, `kernel/fs.c`, `kernel/file.c`, `kernel/sysfile.c`, `kernel/perms.h`, `kernel/perms.c`, `mkfs/mkfs.c`, plus user tools `user/chmod.c` and `user/chown.c`.

## The Permission Bit Model

Unix permission bits pack into a 16-bit `mode` word:

```
  15–12     11–9      8–6       5–3       2–0
  filetype  (unused)  owner     group     other
              rwx       rwx       rwx
```

Each three-bit group:

| Bit position | Meaning |
|-------------|---------|
| `r` (bit 2) | Read access |
| `w` (bit 1) | Write access |
| `x` (bit 0) | Execute access |

Example: `0644` = `rw-r--r--`:
- Owner: read + write
- Group: read only
- Other: read only

## What Changed in `struct dinode`

```c
/* kernel/fs.h: new fields on the on-disk inode */
struct dinode {
  short type;    // existing: T_FILE, T_DIR, T_DEVICE
  short mode;    // NEW: octal permission bits
  short uid;     // NEW: owner user ID
  short gid;     // NEW: owner group ID
  // ... rest unchanged ...
};
```

The in-memory `struct inode` mirrors these fields. `ialloc`, `iupdate`, `ilock`, and `stati` all read and write the new fields.

This is the key difference from original xv6 behavior in this codebase: permission decisions are now based on persistent inode metadata rather than ad hoc assumptions in user space.

## Medical Files and Their Permissions

`mkfs/mkfs.c` creates these files at image build time:

| Path | Mode | Owner | Accessible by |
|------|------|-------|--------------|
| `/patient/records` | `0400` | uid=1 (patient) | Patient read-only, admin override |
| `/dosage/insulin.log` | `0620` | uid=2 (doctor) | Doctor (rw), group write, admin override |
| `/device/config` | `0600` | uid=0 (admin) | Admin only |
| `/audit/syscall.log` | `0400` | uid=0 (admin) | Admin read-only |

## `perm_check()` Flowchart

```mermaid
flowchart TD
    A["perm_check(inode, access_type)"] --> B{caller is admin?}
    B -- yes --> Z["ALLOW: admin bypass"]
    B -- no --> C{caller uid == inode uid?}
    C -- yes --> D["check owner bits"]
    C -- no --> E{caller gid == inode gid?}
    E -- yes --> F["check group bits"]
    E -- no --> G["check other bits"]
    D --> H{bit set?}
    F --> H
    G --> H
    H -- yes --> Z
    H -- no --> Y["DENY → return -EACCES"]

    style Z fill:#14532d,color:#bbf7d0
    style Y fill:#7f1d1d,color:#fecaca
```

## The Four Enforcement Points

Every file access in xv6 passes through one of these four kernel locations:

| Hook | File | When it runs |
|------|------|-------------|
| `sys_open()` | `kernel/sysfile.c` | Before any file descriptor is created |
| `fileread()` | `kernel/file.c` | On every `read()` syscall |
| `filewrite()` | `kernel/file.c` | On every `write()` syscall |
| `sys_exec()` | `kernel/exec.c` | Before loading a program image |

Checking only at `open` is insufficient: an attacker with an already-open fd could still read/write after their permission is revoked.

Compared with stock xv6, this closes a common teaching-kernel gap by validating access both when descriptors are opened and when I/O actually occurs.

## `chmod` and `chown` Syscalls

```bash
# Inside xv6 shell (admin logged in)

# Restrict config to admin-only
chmod /device/config 0600

# Transfer ownership of a log file
chown /dosage/insulin.log 1 1
```

`chmod` allows admin or file owner to change mode bits. `chown` is stricter and requires admin (`uid == 0`) in the current implementation.

These syscall entry points are additions in this repository and are wired through the expanded syscall table, so permissions are controlled in kernel space rather than only by convention in user programs.

## Compliance Coverage

![QEMU terminal showing patient denied on /device/config and allowed on /patient/records](/img/screenshots/phase2-perm-denied.png)

| Test | What it checks |
|------|---------------|
| T07 | Patient cannot open `/device/config` (EACCES) |
| T08 | Patient can read `/patient/records` |
| T09 | Patient cannot write `/patient/records` (EACCES) |
| T10 | Clinician can write `/dosage/insulin.log` |
| T11 | Clinician cannot read `/device/config` (EACCES) |
| T12 | Admin can open all protected files |

## Known Implementation Detail: `DIRSIZ`

The filename `compliance_test` is 16 characters. The original xv6 `DIRSIZ` is 14. We increased it to 16:

```c
/* kernel/fs.h */
#define DIRSIZ 16
```

This required updating `mkfs.c` to pad directory entries to the new size so that `ls` and directory reads remain aligned to `struct dirent`.

