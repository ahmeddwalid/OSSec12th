---
sidebar_position: 6
title: Bonus Compliance Testing
---

# Bonus Compliance Testing

## What and Why

The bonus `compliance_test` program is an automated report that exercises all three project phases. It is designed for quick grading and quick regression checks after kernel changes.

The test runs inside xv6, switches identities with the login syscall, performs allowed and denied operations, reads audit data as admin, and prints a final report.

## Theory

A good compliance test should check behavior, not just code presence. For this project that means testing the end-to-end syscall path:

- Authentication should accept valid credentials and reject invalid credentials.
- Permission checks should deny sensitive files to the wrong role.
- Audit logs should record both denied and successful security-relevant events.
- The phases should still work together in one booted system.

Because xv6 has a small user library, the test avoids host-style conveniences and uses only xv6-supported calls.

## Implementation Walk-through

The source file is `user/compliance_test.c`. It uses small helpers for open/read/write checks and an in-memory array of `struct audit_entry` values.

The Makefile includes the program in `UPROGS`:

```make
$U/_compliance_test\
```

The test suite contains 18 high-level checks:

| Range | Coverage |
| --- | --- |
| T01-T06 | login, wrong-password rejection, admin-only account operations, whoami |
| T07-T12 | protected medical files and admin override |
| T13-T17 | admin-only audit reads and audit evidence |
| T18 | all phases active together |

The final report includes the student name and ID and should show `Passed: 18 / 18`.

## How to Test

Build and boot:

```bash
cd xv6-security
make clean
make
make qemu-nox
```

Log in as admin:

```text
Username: admin
Password: admin123
```

Run:

```sh
compliance_test
```

Expected final output:

```text
COMPLIANCE REPORT - CCY4304 12th Project
Student: Ahmed Walid - 221011183
Passed: 18 / 18
```

## Common Pitfalls

The audit ring can fill quickly because trap audit printing itself triggers write syscalls. The compliance test creates fresh audit events near the checks that need them, which keeps the result deterministic.

Do not use C library functions that xv6 does not declare. The test uses `memcmp` for substring checks instead of relying on a host libc routine.

The filename `compliance_test` required increasing `DIRSIZ` from 14 to 16. That filesystem change must be paired with mkfs layout fixes.
