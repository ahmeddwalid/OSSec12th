# xv6 Medical Device Security

This directory contains the modified xv6-riscv source tree for the CCY4304 12th Project. The implementation keeps the original xv6 teaching style while adding medical-device security controls in three phases.

## What Changed

- Authentication: secure login replaces direct shell startup, with kernel-backed account syscalls and seeded demo accounts.
- File permissions: inodes store `mode`, `uid`, and `gid`, and file access checks enforce owner, group, and other bits.
- Audit logging: syscalls are recorded in a fixed-size kernel ring buffer and can be dumped by an admin user.
- Medical demo data: mkfs creates protected files under `/patient`, `/dosage`, `/device`, and `/audit`.
- Compliance testing: `compliance_test` exercises authentication, permissions, audit logging, and cross-phase integration.

## Build

```bash
make clean
make
```

The Makefile auto-detects the RISC-V tool prefix. This project was validated with `riscv64-linux-gnu-*` tools and QEMU RISC-V system emulation.

## Run

```bash
make qemu-nox
```

`qemu-nox` is an alias for the existing nographic `qemu` target in this xv6 snapshot.

Demo accounts:

| User | Password | Role |
| --- | --- | --- |
| `admin` | `admin123` | administrator |
| `patient1` | `patient123` | patient |
| `doctor1` | `doctor123` | clinician |

## Useful Commands Inside xv6

```sh
whoami
useradd name password role
userdel name
passwd name newpassword
chmod path mode
chown path uid gid
perm_test
audit_dump
compliance_test
```

`audit_dump` and account-management syscalls require the admin role. `compliance_test` should finish with `Passed: 18 / 18`.

## Key Files

- `kernel/auth.c`, `kernel/auth.h`: authentication, password hashing, and account management.
- `kernel/perms.c`, `kernel/perms.h`: permission checks.
- `kernel/audit.c`, `kernel/audit.h`: syscall audit ring buffer.
- `kernel/sysfile.c`: syscall wrappers for auth, chmod, chown, open, exec, and audit reads.
- `kernel/fs.c`, `kernel/fs.h`, `kernel/file.h`, `kernel/stat.h`: inode metadata support.
- `mkfs/mkfs.c`: initial filesystem population and metadata.
- `user/login.c`: secure login shell entry point.
- `user/audit_dump.c`: admin audit viewer.
- `user/compliance_test.c`: automated 18-test compliance report.
