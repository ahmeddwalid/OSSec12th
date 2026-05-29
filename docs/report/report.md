# Abstract

This report describes a security layer we added to xv6-riscv, the teaching kernel from MIT. The
setting is a medical wearable, an insulin pump, that has to decide who is using it and what they are
allowed to do. We built three things into the kernel: user authentication at boot, Unix-style file
permissions on every file, and an audit log that records security-relevant system calls.

The motivation is a real failure. In 2019 the FDA issued a Class I recall for the Medtronic MiniMed
508 insulin pump. Researchers showed that anyone within radio range could send commands to the pump
and change the insulin dose, with no authentication at all (CVE-2019-10964). The root cause was an
operating system with no idea of user identity, no file access control, and no record of what
happened. Any process could do anything. Our project answers a narrower question: what is the
smallest set of OS-level controls a device like that needs before it should be trusted with a dose.

We kept the code small on purpose. xv6 is about ten thousand lines, so every change we made stays
readable and every security decision can be traced from the system call down to the kernel check.
The work is split into three phases. Each phase is a self-contained kernel change, and together they
cover identity, access control, and accountability. An 18-test compliance program runs the whole
thing on real xv6 inside QEMU and reports the result.

The three default accounts model the three roles a clinic deals with:

| Username | Password | Role | UID |
|----------|----------|------|-----|
| `admin` | `admin123` | Administrator | 0 |
| `patient1` | `patient123` | Patient | 1 |
| `doctor1` | `doctor123` | Doctor | 2 |

# Environment and Build

The kernel targets the RISC-V 64-bit `virt` board and runs under QEMU. Two tools are needed: a
RISC-V cross compiler and `qemu-system-riscv64`. On Fedora the packages are
`gcc-riscv64-linux-gnu` and `qemu-system-riscv`. On Debian or Ubuntu they are
`gcc-riscv64-linux-gnu` and `qemu-system-misc`.

Building and booting is three commands:

```bash
cd xv6-security
make clean
make
make qemu
```

`make` cross-compiles the kernel and the user programs, then builds the filesystem image with
`mkfs`. `make qemu` boots that image. The console drops straight into the login program, not a
shell, which is the first visible result of Phase 1.

<figure>
  <img src="img/screenshots/qemu-boot-login.png" alt="QEMU console booting xv6 and showing the login prompt"/>
  <figcaption>xv6 boots under QEMU and stops at the secure login prompt instead of a shell.</figcaption>
</figure>

To leave QEMU, press `Ctrl-A` then `x`.

# Phase 1: User Authentication

Stock xv6 runs `sh` the moment it boots. Every process is trusted the same, which is exactly the
hole the MiniMed pump had. Phase 1 puts an identity check in front of the shell.

## What we added to the kernel

Each process now carries an identity. We added five fields to `struct proc`:

```c
int uid;             // 0 = admin, 1 = patient, 2 = doctor
int gid;             // group id, mirrors the uid here
int role;            // ROLE_ADMIN, ROLE_PATIENT, ROLE_DOCTOR
char username[16];   // for whoami and the audit log
int authenticated;   // 0 until login() succeeds
```

A forked child inherits all five fields, so every program a logged-in shell starts runs under the
same identity.

Credentials live in `/etc/passwd`, one account per line:

```
username|uid|gid|role|hash
```

Passwords are never stored in the clear. We hash them first. The hash is a djb2-style function that
runs four 32-bit accumulators to produce a 32-character digest. It is deterministic and needs no
crypto library, which suits a teaching kernel. It is not safe for real use: there is no salt and it
is fast to brute-force. A production device would use bcrypt or Argon2 with a per-account salt. The
kernel seeds the three demo accounts into `/etc/passwd` on first boot.

## The login flow

`init` no longer runs the shell. It runs `login`. The login program asks for a username and
password, calls the `login()` system call, and only runs `sh` once the call returns success. After
three failed attempts it stops and locks the device until reboot, which models a wearable that
should not let an attacker keep guessing.

<figure>
  <img src="img/screenshots/phase1-login-sequence.png" alt="Terminal showing a failed login followed by a successful admin login"/>
  <figcaption>A wrong password is rejected, then a correct admin login opens the shell.</figcaption>
</figure>

## The account commands

Four commands manage identity. They map to system calls that enforce their own rules inside the
kernel, so a patient cannot bypass them by crafting a raw call.

```sh
whoami                          # print name, uid, gid, role
useradd nurse nurse123 1        # admin adds a patient-role account
passwd doctor1 doctor123 newpw  # doctor1 changes its own password
userdel nurse                   # admin removes the account
```

`useradd` and `userdel` are admin only. The `admin` account cannot be deleted. `passwd` lets a normal
user change their own password if they give the old one, and lets the admin change anyone's. `whoami`
works for any logged-in user.

# Phase 2: File Access Control

Phase 1 says who you are. Phase 2 says what you can touch. Without it, a logged-in patient could open
the device config or overwrite the insulin log, which would defeat the login entirely. We added
Unix-style discretionary access control to every file.

## What we added to the inode

The on-disk inode (`struct dinode`) gained three fields, mirrored on the in-memory inode:

```c
ushort mode;   // permission bits, e.g. 0640
ushort uid;    // owner user id
ushort gid;    // owner group id
```

The permission bits follow the standard owner, group, other layout. A single helper, `perm_check`,
reads the caller's identity and the inode's owner, group, and mode, then returns allow or deny. The
admin (uid 0) bypasses the check. Everyone else is classified as owner, group, or other, and the
matching read, write, or execute bit decides the outcome.

## The protected medical files

`mkfs` assigns ownership when it builds the image, so the files are protected from the first boot:

| Path | Mode | Owner | Who can do what |
|------|------|-------|-----------------|
| `/patient/records` | `0400` | uid=1, gid=1 | Patient reads, admin override |
| `/dosage/insulin.log` | `0640` | uid=2, gid=1 | Doctor (owner) writes, patient (group) reads, admin override |
| `/device/config` | `0600` | uid=0, gid=0 | Admin only |
| `/audit/syscall.log` | `0400` | uid=0, gid=0 | Admin read-only |

The insulin log is the interesting one. The doctor owns it and writes new doses. The patient sits in
the file's group with read-only group bits, so they can see their dose history but never change it.
Nobody else gets in.

## Where the check runs

Checking only at open time is not enough, because a process could hold an open handle after its
rights change. So `perm_check` runs at four points: `sys_open`, `fileread`, `filewrite`, and
`sys_exec`. Read needs the read bit, write needs the write bit, and running a program needs the
execute bit.

## Changing permissions

```sh
chmod /device/config 0600       # owner or admin may change the mode
chown /dosage/insulin.log 1 1   # admin only: change owner uid and gid
```

`chmod` is allowed for the file owner or the admin. `chown` is stricter and requires the admin.

## Walkthrough: a patient opens the device config

Say `patient1` (uid 1) runs `cat /device/config`. The file is owned by admin (uid 0) with mode
`0600`. The kernel resolves the path, then calls `perm_check` for read before it hands back a file
descriptor. The patient is not admin, so there is no bypass. The file owner is uid 0, not 1, so the
patient is not the owner. The file group is 0, the patient's group is 1, so they are not in the
group either. That leaves the "other" class, and the other-read bit in `0600` is clear. The check
returns deny, the open returns -1, and the read fails. The attempt is also written to the audit log,
which Phase 3 covers.

<figure>
  <img src="img/screenshots/phase2-perm-denied.png" alt="Terminal showing a patient denied on /device/config but allowed on /patient/records"/>
  <figcaption>The patient is blocked from the device config but can read their own records.</figcaption>
</figure>

# Phase 3: Syscall Audit Log

After an incident, you need to know what happened, when, and who did it. A clinician should not be
able to deny writing a dose, and an intrusion should leave a trail. Phase 3 adds a kernel audit log
that records security-relevant system calls.

## The ring buffer

The log is a fixed ring buffer of 256 entries (`AUDIT_BUF_SIZE`). A ring buffer fits kernel space
well: it needs no dynamic allocation, each write is a couple of pointer moves, and when it fills the
oldest entry is dropped so the system keeps running. A spinlock protects it from concurrent writes.

Each entry holds:

```c
struct audit_entry {
  int  pid;          // process id of the caller
  int  uid;          // user id of the caller
  int  syscall_no;   // which syscall
  int  result;       // return value (-1 means denied or failed)
  uint tick;         // kernel tick, for ordering
  char comm[16];     // process name
};
```

The hook lives in `syscall.c`, right after each system call returns, so both allowed and denied
calls are recorded by the same mechanism. We do not print audit lines from the trap handler. Printing
there would loop, because `printf` itself calls `write`, and each character written would log another
trap that prints again.

## Reading the log

Reading the log is privileged. The `audit_read` system call returns -1 for any caller that is not
the admin (uid 0). It does not even reveal the buffer size to an unprivileged process. Audit data is
treated as sensitive on its own. The `audit_dump` tool prints the buffer for the admin:

```sh
$ audit_dump
tick pid uid syscall result comm
12 3 0 login 0 login
45 4 1 open 3 sh
67 4 1 open -1 compliance
89 4 0 audit_read 0 compliance
```

The columns are tick, pid, uid, syscall, result, and process name. A `-1` on `open` is a denied
access. The line with uid 1 and result -1 is a patient who tried to open a file they do not own.

<figure>
  <img src="img/screenshots/phase3-audit-dump.png" alt="Terminal showing the audit_dump table of ring buffer entries"/>
  <figcaption>The admin dumps the audit ring and sees the denial recorded with its uid and tick.</figcaption>
</figure>

# Compliance Testing

`compliance_test` is a single user program that exercises all three phases on real xv6 inside QEMU.
It logs in as each role, tries actions that should pass and actions that should fail, and checks the
audit log for the right entries. Run it from the shell after logging in:

```sh
compliance_test
```

The 18 tests and what each one checks:

| Test | What it checks |
|------|----------------|
| T01 | Valid admin login succeeds |
| T02 | Valid patient login succeeds |
| T03 | Valid doctor login succeeds |
| T04 | Wrong password is rejected |
| T05 | Non-admin cannot call `useradd` |
| T06 | `whoami` returns the correct username |
| T07 | Patient cannot open `/device/config` |
| T08 | Patient can read `/patient/records` |
| T09 | Patient cannot write `/patient/records` |
| T10 | Doctor can write `/dosage/insulin.log` |
| T11 | Doctor cannot read `/device/config` |
| T12 | Admin can open all protected files |
| T13 | `audit_read` by a non-admin returns EPERM |
| T14 | `audit_read` by the admin returns data |
| T15 | The log contains the denial event |
| T16 | The log contains the successful write event |
| T17 | An attack is denied and detected in the audit log |
| T18 | All three phases are active at once |

All 18 pass.

<figure>
  <img src="img/screenshots/compliance-full-pass.png" alt="Terminal showing compliance_test reporting 18 of 18 tests passed"/>
  <figcaption>The compliance run reports 18 of 18 tests passed.</figcaption>
</figure>

# Regulatory Context

This is a teaching model, not a compliance claim. Still, the three phases line up with what real
guidance asks for. The FDA's 2023 premarket cybersecurity guidance expects authentication, access
control, and event logging on a connected medical device. IEC 62443, the industrial security
standard often applied to medical devices, frames the same ideas as identification and
authentication control, use control, and the recording of security events. Phase 1 covers identity,
Phase 2 covers use control, and Phase 3 covers the audit trail. The MiniMed 508 recall is what
happens when all three are missing.

# Appendix A: Source Code

The core of each phase is listed below. The full source, including the user-space tool wrappers
(`useradd`, `userdel`, `passwd`, `whoami`, `chmod`, `chown`, `audit_dump`) and the compliance test,
is on GitHub at <https://github.com/ahmeddwalid/OSSec12th>.

## Phase 1: kernel/auth.c

<!-- INCLUDE ../xv6-security/kernel/auth.c -->

## Phase 1: user/login.c

<!-- INCLUDE ../xv6-security/user/login.c -->

## Phase 2: kernel/perms.c

<!-- INCLUDE ../xv6-security/kernel/perms.c -->

## Phase 2: medical file ownership in mkfs/mkfs.c

<!-- INCLUDE ../xv6-security/mkfs/mkfs.c 294 306 -->

## Phase 3: kernel/audit.c

<!-- INCLUDE ../xv6-security/kernel/audit.c -->
