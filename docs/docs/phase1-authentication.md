---
sidebar_position: 3
title: Phase 1 Authentication
---

# Phase 1 Authentication

## What and Why

Phase 1 adds user identity to xv6. Instead of booting directly into a shell, xv6 starts a secure login program. A user must authenticate before reaching the shell, and the kernel stores that identity in the process table.

For a medical device, this is the first control boundary. A patient, clinician, and administrator should not be treated as the same actor.

## Theory

Authentication answers two questions:

- Who is running this process?
- What role should the kernel assign to that identity?

The implementation stores identity in `struct proc`: `uid`, `gid`, `role`, `username`, and an `authenticated` flag. Forked children inherit credentials, so the shell and programs launched from it keep the logged-in identity.

Passwords are hashed before storage in `/etc/passwd`. The hash is intentionally simple for xv6, but the design keeps plaintext passwords out of the account file. A production system would use a memory-hard password hashing algorithm and proper salt handling.

## Implementation Walk-through

The authentication subsystem lives in `kernel/auth.c` and `kernel/auth.h`. It seeds these demo accounts:

| User | Password | Role |
| --- | --- | --- |
| `admin` | `admin123` | administrator |
| `patient1` | `patient123` | patient |
| `doctor1` | `doctor123` | clinician |

The user entry point changes in `user/init.c`. It now starts `login` instead of `sh`. The login program prompts for username and password, calls the `login()` syscall, and executes `sh` only after success.

The syscall surface includes:

- `login(username, password)`
- `useradd(username, password, role)`
- `userdel(username)`
- `passwd(username, newpassword)`
- `whoami(buffer, size)`

Admin-only management is enforced inside the kernel, not only in user tools.

## How to Test

Boot xv6:

```bash
cd xv6-security
make qemu-nox
```

Try a valid login:

```text
Username: admin
Password: admin123
```

Inside xv6, run:

```sh
whoami
```

Then run the compliance checks that cover authentication:

```sh
compliance_test
```

Tests T01 through T06 cover valid logins, wrong password rejection, non-admin account management denial, and `whoami` output.

## Common Pitfalls

Do not rely on user-space tools alone for authorization. A user can bypass a friendly command wrapper by calling a syscall directly. The kernel must enforce admin-only account operations.

Remember to copy credentials on fork. If the child shell loses identity, later permission checks and audit entries become misleading.

Avoid storing plaintext passwords in the filesystem. Even in xv6, the project should model safer habits.
