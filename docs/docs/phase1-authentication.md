---
sidebar_position: 3
title: Phase 1 Authentication
---

# Phase 1: Authentication

## Motivation

Stock xv6 calls `exec("sh", ...)` in `init.c` and the shell runs immediately with no identity. Any process is implicitly trusted equally. For a medical device this is catastrophic: a patient application could call any syscall, read any file, modify any device configuration.

Phase 1 imposes an identity boundary at the earliest possible moment, before the first shell command executes.

## Original xv6 vs Phase 1 Code Delta

Stock xv6 has no login step and no kernel-backed user identity. This phase introduces both.

| Component | Stock xv6-riscv | Modified xv6-security |
|-----------|------------------|------------------------|
| Boot program | `user/init.c` starts `sh` directly | `user/init.c` starts `login` first |
| Process credentials | `struct proc` has no user fields | Added `uid`, `gid`, `role`, `username`, `authenticated` |
| Auth backend | No credential database | `/etc/passwd` seeded at boot via `auth_init()` |
| Auth syscalls | None | `sys_login`, `sys_whoami`, `sys_useradd`, `sys_userdel`, `sys_passwd` |
| Fork behavior | Child inherits memory/files only | Child also inherits identity fields (`uid/gid/role/username/authenticated`) |

Files touched for this phase: `kernel/proc.h`, `kernel/proc.c`, `kernel/auth.h`, `kernel/auth.c`, `kernel/sysfile.c`, `kernel/syscall.c`, `kernel/syscall.h`, `user/init.c`, `user/login.c`.

## What Changed in `struct proc`

Before Phase 1, `struct proc` had no user-identity fields at all. After Phase 1:

```c
/* kernel/proc.h: new fields */
struct proc {
  // ... existing fields ...
  int uid;                   // numeric user ID
  int gid;                   // numeric group ID
  int role;                  // ROLE_ADMIN / ROLE_DOCTOR / ROLE_PATIENT
  char username[16];         // for audit and whoami
  int authenticated;         // 0 until login() syscall succeeds
};
```

Forked children inherit all five fields, so every descendant of a logged-in shell carries the same identity.

Compared to stock xv6, this is the foundational kernel ABI change for user identity. Later phases (permissions and audit) rely on these fields rather than on user-space trust.

## The `/etc/passwd` Format

```
username|uid|gid|role|hash
```

| Field | Example | Notes |
|-------|---------|-------|
| `username` | `admin` | Max 15 chars |
| `uid` | `0` | numeric user ID |
| `gid` | `0` | group ID, mirrors the uid here |
| `role` | `0` | 0=admin, 1=patient, 2=doctor |
| `hash` | `a3f8...` | SHA-256 hash, 64 hex chars |

## The Three Roles

The project has exactly three roles. The uid and the role number are the same value.

| Role | uid | What they represent |
|------|-----|---------------------|
| ADMIN | 0 | Full access. Bypasses every file permission check. |
| PATIENT | 1 | Reads their own records. Blocked from device config. |
| DOCTOR | 2 | Writes the insulin log. Cannot read device config. |

Stock xv6 does not ship `/etc/passwd` or account management syscalls. Here, `auth_init()` creates `/etc` and `/etc/passwd` on first boot and seeds three demo users so the secure boot path is immediately testable.

Demo accounts baked into the image:

| Username | Password | Role |
|----------|----------|------|
| `admin` | `admin123` | Administrator |
| `doctor1` | `doctor123` | Doctor |
| `patient1` | `patient123` | Patient |

## Login Sequence

```mermaid
sequenceDiagram
    participant QEMU as QEMU console
    participant login as login (user program)
    participant kernel as kernel / auth.c
    participant shell as sh

    QEMU->>login: exec("/login")
    login->>QEMU: prompt "login: "
    QEMU->>login: username + password
    login->>kernel: login(username, password) ECALL
    kernel->>kernel: read /etc/passwd, hash password
    alt credentials valid
        kernel->>login: return 0 (success)
        kernel-->>kernel: set proc.uid, gid, role, authenticated=1
        login->>shell: exec("/sh")
    else wrong password
        kernel->>login: return -1
        login->>QEMU: "Login failed." + retry (locks after 3)
    end
```

## Syscall Surface

![QEMU terminal showing login failure then successful root login](/img/screenshots/phase1-login-sequence.png)

| Syscall | Who can call | Description |
|---------|-------------|-------------|
| `login(user, pass)` | Anyone | Authenticate. Sets the kernel identity on success. |
| `whoami(buf, len)` | Logged-in user | Copy current `username`, uid, gid, role into the buffer. |
| `useradd(user, pass, role)` | Admin only | Add an account entry to `/etc/passwd`. |
| `userdel(user)` | Admin only | Remove an account entry. The `admin` account cannot be deleted. |
| `passwd(user, old, new)` | Owner or admin | Change a password. A normal user can only change their own and must give the old password. Admin can change anyone's. |

The four account syscalls map to these user commands. Run them from the xv6 shell after logging in:

```sh
whoami                        # print who you are: name, uid, gid, role
useradd nurse nurse123 1      # admin adds a patient-role account (role 1)
passwd doctor1 doctor123 newpw  # doctor1 changes its own password
userdel nurse                 # admin removes the account
```

Compared with original xv6, these syscall numbers are newly assigned in the syscall table and dispatched through `kernel/syscall.c`.

The admin-only restriction is enforced **inside the kernel** (`kernel/auth.c`), not just in the user tool. A patient cannot call `useradd` by constructing a raw ECALL.

## `login.c` Walk-through

```c
// user/login.c (simplified)
int main(void) {
  char user[16], pass[64];
  int failures = 0;
  for (;;) {
    printf("Username: "); read_line(user, sizeof user);
    printf("Password: "); read_line(pass, sizeof pass);

    if (login(user, pass) == 0) {   // ECALL -> sys_login()
      char *argv[] = { "sh", 0 };
      exec("sh", argv);             // only runs on success
    }
    failures++;
    printf("Login failed.\n");
    if (failures >= 3) {            // lock the device after 3 tries
      printf("Device locked after 3 failed attempts.\n");
      for (;;) pause(1000);
    }
  }
}
```

After three failed attempts the login program stops trying and pauses forever. The device is locked until reboot. This models a wearable that should not let an attacker brute-force the PIN.

`exec` overwrites the login process image with the shell. The kernel's `proc` entry keeps `uid`, `gid`, `role`, and `authenticated = 1` across the `exec` because credentials are stored in `struct proc`, not in the user-space image.

In stock xv6, the equivalent control flow is `init -> sh` with no identity gate. The modified flow is `init -> login -> sh` and only transitions to shell after `sys_login` succeeds.

## Compliance Coverage

| Test | What it checks |
|------|---------------|
| T01 | Valid admin login succeeds |
| T02 | Valid patient login succeeds |
| T03 | Valid doctor login succeeds |
| T04 | Wrong password is rejected |
| T05 | `useradd` by a non-admin returns `-EPERM` |
| T06 | `whoami` returns the correct username |

## Security Notes

- Passwords are hashed with SHA-256 before storage in `/etc/passwd`. The implementation is self-contained (~80 lines of C) and needs no external crypto library. **This is not yet production-quality.** The hash has no salt and is fast to brute-force. A real system would use Argon2id or bcrypt with a per-account salt and a tunable work factor.
- Forked children inherit `uid`, `gid`, `role`, `username`, and `authenticated` from the parent in `kfork()`. This is intentional for a Unix-style session model where child processes run under the same logged-in identity.


Remember to copy credentials on fork. If the child shell loses identity, later permission checks and audit entries become misleading.

Avoid storing plaintext passwords in the filesystem. Even in xv6, the project should model safer habits.
