<div align="center">

# CCY4304 — 12th Project: xv6 Medical Device Security

**Ahmed Walid Ibrahim · 221011183** &nbsp;|&nbsp; **Ahmed Mohamed Mahmoud · 221010720**

Lecturer: Prof. Dr. Ayman Adel Abdel-Hamid &nbsp;·&nbsp; TA: Abdelrahman Solyman

[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-00d4aa?style=flat-square)](https://ahmeddwalid.github.io/OSSec12th/docs)
[![Compliance](https://img.shields.io/badge/compliance-18%2F18%20PASS-14532d?style=flat-square)](#compliance-results)

</div>

---

## Overview

This repository extends **xv6-riscv** with three security phases to demonstrate OS-level security controls relevant to connected medical devices. The scenario is inspired by the 2019 Medtronic MiniMed 508 insulin pump recall (CVE-2019-10964), where lack of authentication on a wireless interface allowed remote manipulation of insulin doses.

| Phase | Feature | Kernel files |
|-------|---------|-------------|
| 1 | Login authentication + identity in `struct proc` | `kernel/auth.c`, `kernel/sysproc.c` |
| 2 | Unix-style file permissions (mode/uid/gid on inodes) | `kernel/fs.c`, `kernel/perms.c`, `kernel/sysfile.c` |
| 3 | Kernel audit ring buffer (256 entries, spinlock-protected) | `kernel/audit.c`, `kernel/trap.c` |

## Repository Layout

```
.
├── xv6-security/        Modified xv6-riscv kernel + user programs
│   ├── kernel/          auth.c, perms.c, audit.c, fs.c (modified)
│   ├── user/            login.c, audit_dump.c, compliance_test.c
│   └── mkfs/            mkfs.c (creates medical demo files)
├── docs/                Docusaurus documentation site
│   ├── docs/            Markdown pages for all 7 sections
│   └── src/pages/       Landing page (index.tsx)
└── README.md            This file
```

## Quick Start

### 1. Install Toolchain (Fedora)

```bash
sudo dnf install make gcc perl python3 bc qemu-system-riscv-core \
                 gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu \
                 nodejs npm
```

### 2. Build xv6

```bash
cd xv6-security
make clean && make
```

### 3. Boot in QEMU

```bash
make qemu        # with graphics
# or
make qemu-nox    # terminal only
```

### 4. Log In

At the secure login prompt:

| Username | Password | Role |
|----------|----------|------|
| `root` | `root123` | Administrator |
| `admin` | `admin123` | Administrator |
| `doctor1` | `doctor123` | Clinician |
| `patient1` | `patient123` | Patient |

### 5. Run Compliance Tests

```sh
compliance_test
audit_dump
```

## Compliance Results

```
COMPLIANCE REPORT — CCY4304 12th Project
Team: Ahmed Walid Ibrahim (221011183) & Ahmed Mohamed Mahmoud (221010720)
─────────────────────────────────────────────────────────────
[PASS] T01 valid_admin_login
[PASS] T02 valid_patient_login
[PASS] T03 wrong_password_rejected
[PASS] T04 whoami_returns_username
[PASS] T05 useradd_by_patient_denied
[PASS] T06 userdel_by_patient_denied
[PASS] T07 patient_cannot_open_config
[PASS] T08 patient_can_read_records
[PASS] T09 patient_cannot_write_records
[PASS] T10 doctor_can_write_dosage
[PASS] T11 doctor_cannot_read_config
[PASS] T12 admin_can_open_all
[PASS] T13 patient_cannot_read_audit
[PASS] T14 admin_can_read_audit
[PASS] T15 audit_records_denied_open
[PASS] T16 audit_records_successful_write
[PASS] T17 end_to_end_attack_detection
[PASS] T18 all_phases_active
─────────────────────────────────────────────────────────────
Passed: 18 / 18   Failed: 0 / 18
```

## Documentation Site

The full documentation is hosted on GitHub Pages. To build locally:

```bash
cd docs
npm install
npm run build
npm run serve    # preview at http://localhost:3000
```

Online: [https://ahmeddwalid.github.io/OSSec12th/docs](https://ahmeddwalid.github.io/OSSec12th/docs)

## Toolchain Versions

| Tool | Version used |
|------|-------------|
| `riscv64-linux-gnu-gcc` | 15.2.1 |
| `qemu-system-riscv64` | 10.2.2 |
| `make` | 4.4.1 |
| Node.js | 22.x |

## Stop QEMU

Press `Ctrl-A` then `X`.


After GitHub Pages deployment, the documentation is expected at:

https://ossec.ahmeddwalid.me/

## Quality Gates

The final verification path for this submission is:

```bash
cd xv6-security
make clean
make 2>&1 | grep -E "error:|warning:"
make qemu-nox
```

Inside xv6, log in as `admin/admin123`, run `compliance_test`, and confirm `Passed: 18 / 18`. Then run `audit_dump` to inspect syscall audit entries.
