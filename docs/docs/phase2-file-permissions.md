---
sidebar_position: 4
title: Phase 2 File Permissions
---

# Phase 2 File Permissions

## What and Why

Phase 2 adds Unix-style file permissions to xv6. Files and directories now carry a mode, owner uid, and group gid. Kernel file operations consult that metadata before allowing reads, writes, or execution.

This matters for the medical-device scenario because patient records, dosage logs, device configuration, and audit files have different sensitivity levels.

## Theory

Unix permission bits divide access into owner, group, and other classes. Each class can have read, write, and execute bits. A process chooses the relevant class by comparing its uid and gid with the inode metadata.

The project models this with mode values such as:

- Owner read, write, execute.
- Group read, write, execute.
- Other read, write, execute.

Administrators bypass permission checks so recovery and inspection remain possible.

## Implementation Walk-through

The on-disk inode and in-memory inode were extended with:

```c
short mode;
short uid;
short gid;
```

The user-visible `stat` structure exposes the same metadata. The filesystem reads and writes these fields through `ialloc`, `iupdate`, `ilock`, and `stati`.

Permission logic lives in `kernel/perms.c` and `kernel/perms.h`. The helper chooses owner, group, or other bits and checks the requested access type.

The syscall path enforces permissions in several places:

- `sys_open()` checks requested read/write access.
- `fileread()` checks read access.
- `filewrite()` checks write access.
- `sys_exec()` checks executable access before loading a program.
- `chmod()` and `chown()` update metadata through kernel syscalls.

`mkfs/mkfs.c` creates demo medical files with metadata:

- `/patient/records`
- `/dosage/insulin.log`
- `/device/config`
- `/audit/syscall.log`

## How to Test

Run the permission test tools inside xv6:

```sh
perm_test
compliance_test
```

The compliance suite checks the main access-control cases:

- Patient cannot open `/device/config`.
- Patient can read `/patient/records`.
- Patient cannot write `/patient/records`.
- Doctor can write `/dosage/insulin.log`.
- Doctor cannot read `/device/config`.
- Admin can open all protected files.

## Common Pitfalls

Changing inode and directory structures changes the filesystem image layout. The project increased `DIRSIZ` to fit `compliance_test`, then updated mkfs padding so directory reads stay aligned to `struct dirent`.

Another common mistake is checking permissions only in `open`. Existing file descriptors can still be used later, so reads and writes also need kernel-level checks.

Do not forget executable checks. A medical-device shell should not run a file simply because it can read it.
