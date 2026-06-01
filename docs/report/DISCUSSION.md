# Project Discussion: xv6 Medical Device Security (CCY4304 — 12th Project)

**Ahmed Walid Ibrahim (221011183) & Jana Ashraf Ali (221010291)**

---

## Table of Contents

1. [Environment Setup and Build](#1-environment-setup-and-build)
2. [Phase 1 — User Authentication](#2-phase-1--user-authentication)
3. [Phase 2 — File Access Control](#3-phase-2--file-access-control)
4. [Phase 3 — Syscall Audit Log](#4-phase-3--syscall-audit-log)
5. [Ring Buffer Deep-Dive](#5-ring-buffer-deep-dive)
6. [Full Walkthrough: Step-by-Step Commands](#6-full-walkthrough-step-by-step-commands)
7. [Compliance Testing](#7-compliance-testing)
8. [Command Reference Table](#8-command-reference-table)

---

## 1. Environment Setup and Build

### 1.1 Install Dependencies

**Fedora / RHEL:**
```bash
sudo dnf install make gcc perl python3 bc qemu-system-riscv-core \
                 gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

**Debian / Ubuntu / Kali Linux:**
```bash
sudo apt update
sudo apt install make gcc perl python3 bc qemu-system-misc \
                 gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu
```

**Arch Linux:**
```bash
sudo pacman -S make gcc perl python3 bc qemu-system-riscv \
               riscv64-linux-gnu-gcc riscv64-linux-gnu-binutils
```

### 1.2 Build the Kernel

```bash
cd xv6-security
make clean && make
```

**What happens:** `make` cross-compiles the kernel (auth.o, perms.o, audit.o, and all standard xv6 objects) using `riscv64-linux-gnu-gcc`, then runs `mkfs/mkfs` to build `fs.img` — the RISC-V filesystem image. The `mkfs` tool calls `add_medical_files()` which creates the protected medical directories and files with ownership and permissions baked in at build time.

### 1.3 Boot in QEMU

```bash
make qemu-nox
```

**What to expect:** The kernel boots and `init` (in `user/init.c`) forks and execs `login` instead of `sh`. You will see:

```
init: starting secure login
xv6 Medical Device OS - Secure Login
Username: 
```

Press `Ctrl-A` then `x` to exit QEMU.

---

## 2. Phase 1 — User Authentication

### What Was Changed

**Stock xv6:** Runs `sh` immediately on boot. Every process is untrusted identity.

**Our change:** An authentication gate is placed in front of the shell. `init` execs `login` instead of `sh`. The shell only runs after a successful `login()` system call.

### Key Code: `struct proc` (kernel/proc.h:85-112)

Five fields were added to the per-process structure:

```c
struct proc {
  // ... standard xv6 fields ...
  int uid;                     // 0=admin, 1=patient, 2=doctor
  int gid;                     // group id (mirrors uid)
  int role;                    // ROLE_ADMIN=0, ROLE_PATIENT=1, ROLE_DOCTOR=2
  char username[16];           // set by auth_login
  int authenticated;           // 0=fresh process, 1=login() succeeded
};
```

A forked child inherits all five fields, so every program a logged-in shell starts runs under the same identity.

### Key Code: `auth_login()` (kernel/auth.c:392-414)

```c
int auth_login(char *username, char *password) {
  struct credential creds[MAX_USERS];
  char hash[HASH_LEN + 1];
  struct proc *p = myproc();
  int count = read_passwd(creds, MAX_USERS);

  if(count < 0 || emptystr(username) || emptystr(password))
    return -1;
  pw_hash(password, hash);          // SHA-256 hash the input
  for(int i = 0; i < count; i++) {
    if(streq(username, creds[i].username) && streq(hash, creds[i].hash)){
      p->uid = creds[i].uid;       // assign identity to process
      p->gid = creds[i].gid;
      p->role = creds[i].role;
      safestrcpy(p->username, creds[i].username, sizeof(p->username));
      p->authenticated = 1;
      return 0;                    // success
    }
  }
  return -1;                       // bad credentials
}
```

### Key Code: SHA-256 Hashing (kernel/auth.c:253-352)

`pw_hash()` is a self-contained ~100-line SHA-256 implementation per FIPS 180-4. It takes a plaintext password and outputs a 64-character hex digest. Passwords are never stored in plaintext in `/etc/passwd`.

### Key Code: `/etc/passwd` Format

Credentials are stored pipe-delimited:
```
username|uid|gid|role|sha256hash
```

Example (seeded at boot by `auth_init()`):
```
admin|0|0|0|<sha256 of admin123>
patient1|1|1|1|<sha256 of patient123>
doctor1|2|2|2|<sha256 of doctor123>
```

### Key Code: Login Lockout (user/login.c:51-57)

```c
failures++;
if(failures >= 3){
  printf("Device locked after 3 failed attempts.\n");
  for(;;)
    pause(1000);  // device must be power-cycled
}
```

Three wrong passwords locks the device permanently until reboot. Models brute-force prevention for medical devices.

### Key Code: `init` Launches `login` (user/init.c:12,34)

```c
char *argv[] = { "login", 0 };
// ...
if(pid == 0){
  exec("login", argv);  // NOT "sh"
}
```

### Default Accounts

| Username | Password | Role | UID |
|----------|----------|------|-----|
| `admin` | `admin123` | Administrator | 0 |
| `patient1` | `patient123` | Patient | 1 |
| `doctor1` | `doctor123` | Doctor | 2 |

---

## 3. Phase 2 — File Access Control

### What Was Changed

Without this phase, any logged-in user can read/write every file — making the login from Phase 1 pointless. Phase 2 adds Unix-style discretionary access control (DAC) to every file on the filesystem.

### Key Code: Inode Extensions (kernel/fs.h:32-42)
The on-disk inode gained three fields:

```c
struct dinode {
  short type;
  short major;
  short minor;
  short nlink;
  ushort mode;   // rwx permission bits (e.g. 0640)
  ushort uid;    // owner user id
  ushort gid;    // owner group id
  uint size;
  uint addrs[NDIRECT+1];
};
```

### Key Code: `perm_check()` (kernel/perms.c:36-53)

```c
int perm_check(struct inode *ip, char access) {
  struct proc *p = myproc();
  int class, bit;

  if(p == 0 || p->uid == 0) return 1;   // root bypasses DAC
  if(ip->uid == p->uid)     class = 0;  // file owner
  else if(ip->gid == p->gid) class = 1; // group member
  else                        class = 2; // everyone else
  bit = access_bit(access, class);
  return bit != 0 && (ip->mode & bit) != 0;
}
```

Resolution order: owner match → group match → other. Root (uid 0) always passes.

### Key Code: Four Hook Points

`perm_check` is called at four points in the kernel, not just at open time:

| Hook Point | File | Line | What it checks |
|------------|------|------|----------------|
| `sys_open` | sysfile.c:348-355 | Before returning a file descriptor | read or write bit based on `omode` |
| `fileread` | file.c:123 | Before every `readi` call | read bit |
| `filewrite` | file.c:168 | Before every `writei` call | write bit |
| `sys_exec` | sysfile.c:467 | Before executing a program | execute bit |

Checking at open time alone is insufficient — a process could hold an open handle after its rights change. All four hooks close this gap.

### Protected Medical Files (created by mkfs)

| Path | Mode | Owner UID | Who can do what |
|------|------|-----------|------------------|
| `/patient/records` | `0400` | uid=1, gid=1 | Patient (owner) reads; admin bypasses |
| `/dosage/insulin.log` | `0640` | uid=2, gid=1 | Doctor (owner) writes; patient (group) reads; admin bypasses |
| `/device/config` | `0600` | uid=0, gid=0 | Admin only |
| `/audit/syscall.log` | `0400` | uid=0, gid=0 | Admin read-only |

### Key Code: `sys_chmod` (kernel/sysfile.c:614-641)

```c
// only root or the file owner can change mode bits
if(p->uid != 0 && ip->uid != p->uid){
  iunlockput(ip);
  end_op();
  return -1;
}
ip->mode = mode & 0777;
```

### Key Code: `sys_chown` (kernel/sysfile.c:644-670)

```c
// only root may change file ownership
if(p->uid != 0)
  return -1;
ip->uid = uid;
ip->gid = gid;
```

`chown` is stricter than `chmod` — only admin (uid 0) can change ownership.

### Walkthrough: Patient Tries to Read Device Config

1. `patient1` (uid 1) runs `cat /device/config`
2. Kernel resolves path to inode
3. `sys_open` calls `perm_check(ip, 'r')` for the inode (mode 0600, uid 0, gid 0)
4. Patient is not admin → no bypass
5. Patient is not file owner (uid 0 ≠ 1) → not class 0
6. Patient gid (1) ≠ file gid (0) → not class 1
7. Falls to class 2 (other); other-read bit in 0600 is 0 → **denied**
8. `open()` returns -1
9. The denial is recorded in the audit ring buffer (Phase 3)

---

## 4. Phase 3 — Syscall Audit Log

### What Was Changed

After an incident, you need to know what happened, when, and who did it. Phase 3 adds a kernel audit log that records every security-relevant system call — both allowed and denied.

### Key Code: Audit Entry Structure (kernel/audit.h:7-14)

```c
#define AUDIT_BUF_SIZE 256

struct audit_entry {
  int  pid;          // process id of the caller
  int  uid;          // user id of the caller
  int  syscall_no;   // which syscall
  int  result;       // return value (-1 = denied/failed)
  uint tick;         // kernel tick (uptime proxy)
  char comm[16];     // process name
};
```

### Key Code: The Hook in `syscall()` (kernel/syscall.c:148-167)

```c
void syscall(void) {
  int num;
  uint64 ret;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    ret = syscalls[num]();       // execute the syscall
    p->trapframe->a0 = ret;
  } else {
    ret = -1;
    p->trapframe->a0 = ret;
  }
  audit_log(num, (int)ret);      // ← log AFTER every syscall
}
```

The audit hook is at the very end of `syscall()`, after every system call returns. This means both successful and denied calls are logged by the same mechanism.

### Why Not Print from the Trap Handler?

Printing audit lines from the trap handler would loop: `printf` calls `write`, which is a syscall, which would trigger another audit entry, which would call `printf` again — infinite recursion. The audit_log() function only stores entries in the ring buffer; printing is done separately by the `audit_dump` user program.

### Key Code: Admin-Only Read Gate (kernel/sysfile.c:683-684)

```c
if(bufsz <= 0 || p->uid != 0)    // admin-only
  return -1;
```

Non-admin users cannot even reveal the buffer size. Audit data is treated as sensitive.

---

## 5. Ring Buffer Deep-Dive

### What Is a Ring Buffer?

A ring buffer (circular buffer) is a fixed-size array where writing wraps around to the beginning when it reaches the end. It needs no dynamic memory allocation, each write is a couple of pointer moves, and when it fills the oldest entry is dropped so the system keeps running indefinitely.

### Why a Ring Buffer for Kernel Audit Logging?

1. **No `malloc` in kernel** — xv6 has no heap allocator for the kernel; a fixed array avoids the problem entirely
2. **O(1) write** — each log entry is just a struct copy + two pointer increments
3. **Bounded memory** — exactly 256 entries × sizeof(struct audit_entry) = 256 × 28 bytes = ~7 KB, regardless of how many syscalls fire
4. **Oldest-first eviction** — when full, the oldest entry is silently overwritten so the kernel never blocks or runs out of space
5. **Spinlock protection** — prevents corruption from concurrent writes on multi-core RISC-V (the Makefile runs with 3 cores)

### Key Code: Ring Buffer Variables (kernel/audit.c:9-15)

```c
static struct audit_entry ring[AUDIT_BUF_SIZE];  // 256 entries
static int head;      // next write position
static int tail;      // oldest valid entry
static int count;     // number of live entries (capped at 256)
static struct spinlock audit_lock;
```

### Key Code: Writing to the Buffer — `audit_log()` (kernel/audit.c:31-53)

```c
void audit_log(int syscall_no, int result) {
  struct proc *p = myproc();
  struct audit_entry *e;

  acquire(&audit_lock);
  e = &ring[head];                              // point to write slot
  e->pid = p ? p->pid : 0;
  e->uid = p ? p->uid : -1;
  e->syscall_no = syscall_no;
  e->result = result;
  e->tick = ticks;
  if(p)
    safestrcpy(e->comm, p->name, sizeof(e->comm));
  else
    e->comm[0] = 0;
  head = (head + 1) % AUDIT_BUF_SIZE;           // wrap around
  if(count < AUDIT_BUF_SIZE)
    count++;                                    // buffer not full yet
  else
    tail = (tail + 1) % AUDIT_BUF_SIZE;         // evict oldest
  release(&audit_lock);
}
```

**Step by step:**
1. Acquire the spinlock (prevents concurrent writes from other cores)
2. Copy the current process info into `ring[head]`
3. Advance `head` by 1, wrapping with modulo 256
4. If buffer isn't full, increment `count`
5. If buffer is full, advance `tail` to discard the oldest entry
6. Release the spinlock

### Key Code: Reading the Buffer — `audit_read()` (kernel/audit.c:57-79)

```c
int audit_read(char *buf, int bufsz) {
  struct proc *p = myproc();
  int copied = 0, idx;

  if(p == 0 || p->uid != 0)       // admin-only gate
    return -1;
  acquire(&audit_lock);
  int capacity = bufsz / sizeof(struct audit_entry);
  int nentry = count;
  if(nentry > capacity)
    nentry = capacity;
  idx = (head - nentry + AUDIT_BUF_SIZE) % AUDIT_BUF_SIZE;  // start from oldest
  for(int i = 0; i < nentry; i++) {
    memmove(buf + copied, &ring[idx], sizeof(struct audit_entry));
    copied += sizeof(struct audit_entry);
    idx = (idx + 1) % AUDIT_BUF_SIZE;                       // wrap forward
  }
  release(&audit_lock);
  return copied;
}
```

**Step by step:**
1. Admin gate: only uid 0 can read
2. Calculate how many entries fit in the caller's buffer
3. Start from the oldest entry: `idx = (head - nentry + 256) % 256`
4. Copy entries forward from oldest to newest
5. Return total bytes written

### Visual Example

```
Buffer state after 258 writes (256 entries, 2 overwrites):

  tail          (entries 2-257 overwritten)
   ↓
   [  2 ][  3 ][  4 ] ... [255 ][256 ][257 ]
   ↑
   head (next write goes here)

audit_read() returns entries 2 through 257, in order.
```

---

## 6. Full Walkthrough: Step-by-Step Commands

### 6.1 Build and Boot

```bash
cd xv6-security
make clean && make
make qemu-nox
```

**Expected output:**
```
init: starting secure login
xv6 Medical Device OS - Secure Login
Username: 
```

---

### 6.2 Login as Admin

At the prompt:
```
Username: admin
Password: admin123
```

**Expected output:** You enter the shell. The process now has uid=0, gid=0, role=0 (admin).

```
$ 
```

---

### 6.3 Check Identity with `whoami`

```
$ whoami
admin uid=0 gid=0 role=0
```

**What happens:** The `whoami` user program calls the `whoami()` syscall, which calls `auth_whoami()` in the kernel. It reads the current process's `uid`, `gid`, `role`, and `username` fields and formats them into a string.

---

### 6.4 Add a New User with `useradd`

```
$ useradd nurse nurse123 1
```

**What happens:** Calls `useradd()` syscall → `auth_useradd()`. Only admin (uid 0) can run this. The kernel hashes the password with SHA-256 and appends a new line to `/etc/passwd`.

**Expected output:** No output on success. The account `nurse` now exists with uid=1, gid=1, role=1 (patient role).

**Verify by logging in as nurse:**
```
$ exit
```
(You return to the login prompt.)

```
Username: nurse
Password: nurse123
```

```
$ whoami
nurse uid=1 gid=1 role=1
```

---

### 6.5 Test Permission Denied: Non-Admin Cannot Add Users

Log in as `patient1`:
```
$ exit
Username: patient1
Password: patient123
```

```
$ useradd hacker hacker123 1
useradd: failed
```

**What happens:** The kernel checks `p->uid != 0` in `auth_useradd()` and returns -1. A patient cannot create accounts.

---

### 6.6 Change Password with `passwd`

Log in as `doctor1`:
```
$ exit
Username: doctor1
Password: doctor123
```

Change your own password:
```
$ passwd doctor1 doctor123 doctor456
```

**Expected output:** No output on success. The kernel hashes the new password and overwrites the hash in `/etc/passwd`.

**What happens:** Non-admin users must provide the correct old password. The kernel verifies the old hash before accepting the new one. Admin can change anyone's password without knowing the old one.

---

### 6.7 File Permission Tests

Log in as admin:
```
$ exit
Username: admin
Password: admin123
```

**Admin can read the device config:**
```
$ cat /device/config
Device config: basal_rate=1.0 safety_lock=on
```

**Admin can read patient records:**
```
$ cat /patient/records
Patient record: glucose stable, follow-up required.
```

**Admin can read the insulin log:**
```
$ cat /dosage/insulin.log
Insulin dosage log initialized.
```

Now log in as patient:
```
$ exit
Username: patient1
Password: patient123
```

**Patient CAN read their own records (mode 0400, owner uid=1):**
```
$ cat /patient/records
Patient record: glucose stable, follow-up required.
```

**Patient CANNOT read device config (mode 0600, owner uid=0):**
```
$ cat /device/config
```

**Expected output:** Empty or error — the open() call returned -1 because `perm_check` denied the read.

**Patient CANNOT write their own records (mode 0400 — read-only):**
```
$ echo test > /patient/records
```

**Expected output:** Write fails because write permission bit is not set for the owner.

---

### 6.8 Change File Permissions with `chmod`

Log in as admin:
```
$ exit
Username: admin
Password: admin123
```

Make the device config readable by group (gid=0):
```
$ chmod /device/config 0640
```

**What happens:** `chmod` syscall → `sys_chmod()` checks that caller is admin or file owner. Sets `ip->mode = 0640 & 0777 = 0640`.

**Expected output:** No output on success.

---

### 6.9 Change File Ownership with `chown`

```
$ chown 1 1 /dosage/insulin.log
```

**What happens:** `chown` syscall → `sys_chown()` requires admin (uid 0). Changes the file's owner uid to 1 and gid to 1.

**Expected output:** No output on success. The insulin log is now owned by patient1 instead of doctor1.

---

### 6.10 Delete a User with `userdel`

```
$ userdel nurse
```

**Expected output:** No output on success. The `nurse` account is removed from `/etc/passwd`.

**Try to delete admin:**
```
$ userdel admin
userdel: failed
```

**What happens:** `auth_userdel()` in the kernel explicitly refuses to delete the admin account:
```c
if(streq(username, "admin"))  // admin account is permanent
  return -1;
```

---

### 6.11 View the Audit Log with `audit_dump`

```
$ audit_dump
```

**Expected output:**
```
tick pid uid syscall result comm
12 3 0 login 0 login
45 4 1 open 3 sh
67 4 1 open -1 compliance
89 4 0 audit_read 0 compliance
...
```

**Columns:** tick (kernel time), pid (process ID), uid (user ID), syscall (name), result (return value; -1 = denied), comm (process name).

**What happens:** `audit_dump` calls `audit_read()` syscall. The kernel checks `p->uid != 0` → denied for non-admin. Only admin can dump the log.

**Key entries to look for:**
- `open -1` = a denied file access attempt
- `audit_read -1` = a non-admin tried to read the audit log
- `login 0` = successful login
- `useradd 0` = successful user creation

---

### 6.12 Non-Admin Cannot Read Audit Log

Log in as patient:
```
$ exit
Username: patient1
Password: patient123
```

```
$ audit_dump
Permission denied.
```

**What happens:** `audit_read()` syscall → kernel checks `p->uid != 0` → returns -1. The `audit_dump` user program prints "Permission denied."

---

### 6.13 Run `perm_test`

Log in as admin:
```
$ exit
Username: admin
Password: admin123
```

```
$ perm_test
```

**Expected output:**
```
[PASS] patient denied /device/config
[PASS] doctor writes insulin log
[PASS] admin opens /device/config
perm_test: 3 passed, 0 failed
```

**What happens:** `perm_test` is a focused test for Phase 2. It logs in as each role, tries specific open/write operations, and verifies the DAC checks work correctly.

---

## 7. Compliance Testing

### Running the Full Test Suite

```
$ compliance_test
```

**Expected output:**
```
[PASS] T01 valid admin login succeeds
[PASS] T02 valid patient login succeeds
[PASS] T03 valid doctor login succeeds
[PASS] T04 wrong password is rejected
[PASS] T05 non-admin cannot call useradd
[PASS] T06 whoami returns correct username
[PASS] T07 patient cannot open /device/config
[PASS] T08 patient can read /patient/records
[PASS] T09 patient cannot write /patient/records
[PASS] T10 doctor can write /dosage/insulin.log
[PASS] T11 doctor cannot read /device/config
[PASS] T12 admin can open all protected files
[PASS] T13 audit_read by non-admin returns EPERM
[PASS] T14 audit_read by admin returns data
[PASS] T15 log contains EPERM denial event
[PASS] T16 log contains successful write event
[PASS] T17 attack denied and detected in audit
[PASS] T18 all three phases active simultaneously

==========================================
  COMPLIANCE REPORT - CCY4304 12th Project
  Students: Ahmed Walid Ibrahim - 221011183
            Jana Ashraf Ali - 221010291
  Passed: 18 / 18
==========================================
```

### Test Breakdown

| Test | Phase | What It Verifies |
|------|-------|------------------|
| T01 | 1 | Admin login succeeds |
| T02 | 1 | Patient login succeeds |
| T03 | 1 | Doctor login succeeds |
| T04 | 1 | Wrong password is rejected |
| T05 | 1 | Non-admin cannot call `useradd` |
| T06 | 1 | `whoami` returns correct username |
| T07 | 2 | Patient cannot open `/device/config` |
| T08 | 2 | Patient can read `/patient/records` |
| T09 | 2 | Patient cannot write `/patient/records` |
| T10 | 2 | Doctor can write `/dosage/insulin.log` |
| T11 | 2 | Doctor cannot read `/device/config` |
| T12 | 2 | Admin can open all protected files |
| T13 | 3 | `audit_read` by non-admin returns EPERM |
| T14 | 3 | `audit_read` by admin returns data |
| T15 | 3 | Log contains the denial event |
| T16 | 3 | Log contains the successful write event |
| T17 | 3 | Attack is denied and detected in audit |
| T18 | All | All three phases active simultaneously |

---

## 8. Command Reference Table

| Command | Syntax | Purpose | Syscall | Who Can Run |
|---------|--------|---------|---------|-------------|
| `whoami` | `whoami` | Print name, uid, gid, role | `SYS_whoami` (25) | Any authenticated user |
| `useradd` | `useradd <name> <pass> <role>` | Create a new account | `SYS_useradd` (22) | Admin only (uid=0) |
| `userdel` | `userdel <name>` | Delete an account | `SYS_userdel` (23) | Admin only (uid=0); cannot delete `admin` |
| `passwd` | `passwd <user> <old> <new>` | Change password | `SYS_passwd` (24) | Any user (own password); admin (anyone's) |
| `chmod` | `chmod <path> <mode>` | Change file mode bits | `SYS_chmod` (27) | File owner or admin |
| `chown` | `chown <uid> <gid> <path>` | Change file owner | `SYS_chown` (28) | Admin only (uid=0) |
| `audit_dump` | `audit_dump` | Dump the audit ring buffer | `SYS_audit_read` (29) | Admin only (uid=0) |
| `perm_test` | `perm_test` | Run Phase 2 permission tests | Multiple | Any authenticated user |
| `compliance_test` | `compliance_test` | Run all 18 compliance tests | Multiple | Any authenticated user |
| `login` | (automatic at boot) | Authenticate the user | `SYS_login` (26) | Runs as init process |
| `cat` | `cat <path>` | Read and print a file | `SYS_open` + `SYS_read` | Subject to DAC |
| `ls` | `ls` | List directory contents | `SYS_open` + `SYS_read` | Subject to DAC |
| `echo` | `echo <text>` | Print text | `SYS_write` | Any |
| `mkdir` | `mkdir <path>` | Create a directory | `SYS_mkdir` (20) | Any |
| `rm` | `rm <path>` | Remove a file | `SYS_unlink` (18) | Subject to DAC |

### System Call Numbers (Added Security Extensions)

| Number | Name | Source |
|--------|------|--------|
| 22 | `SYS_useradd` | kernel/auth.c |
| 23 | `SYS_userdel` | kernel/auth.c |
| 24 | `SYS_passwd` | kernel/auth.c |
| 25 | `SYS_whoami` | kernel/auth.c |
| 26 | `SYS_login` | kernel/sysfile.c |
| 27 | `SYS_chmod` | kernel/sysfile.c |
| 28 | `SYS_chown` | kernel/sysfile.c |
| 29 | `SYS_audit_read` | kernel/sysfile.c |

---

## Summary of Key Files

| File | Purpose |
|------|---------|
| `kernel/auth.c` | Authentication: login, useradd, userdel, passwd, whoami, SHA-256 hash |
| `kernel/auth.h` | Role constants, credential struct, function prototypes |
| `kernel/perms.c` | File permission checking: `perm_check()` |
| `kernel/perms.h` | Unix permission bit constants (0400-0001) |
| `kernel/audit.c` | Ring buffer: init, log, read |
| `kernel/audit.h` | Audit entry struct, AUDIT_BUF_SIZE |
| `kernel/syscall.c` | Syscall dispatch + audit hook on every syscall |
| `kernel/syscall.h` | Syscall numbers 22-29 for security extensions |
| `kernel/sysfile.c` | chmod, chown, audit_read syscalls + permission hooks in open/exec |
| `kernel/file.c` | Permission hooks in fileread and filewrite |
| `kernel/proc.h` | Extended `struct proc` with uid/gid/role/username/authenticated |
| `kernel/fs.h` | Extended `struct dinode` with mode/uid/gid |
| `user/init.c` | Boot process: forks login instead of sh |
| `user/login.c` | Login program: reads credentials, calls login(), locks after 3 failures |
| `user/useradd.c` | User creation CLI wrapper |
| `user/userdel.c` | User deletion CLI wrapper |
| `user/passwd.c` | Password change CLI wrapper |
| `user/whoami.c` | Identity display CLI wrapper |
| `user/chmod.c` | Permission change CLI wrapper |
| `user/chown.c` | Ownership change CLI wrapper |
| `user/audit_dump.c` | Audit log dump (translates syscall numbers to names) |
| `user/perm_test.c` | Phase 2 focused permission tests |
| `user/compliance_test.c` | Full 18-test compliance suite |
| `mkfs/mkfs.c` | Filesystem builder: creates medical files with ownership at build time |
