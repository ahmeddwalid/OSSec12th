# CCY4304 12th Project: xv6 Medical Device Security

Student: Ahmed Walid - 221011183

This repository contains a security-focused xv6-riscv project for CCY4304. It extends xv6 with login-based authentication, Unix-style file permissions, syscall audit logging, protected medical-device demo files, and an automated compliance runner.

## Repository Layout

- `xv6-security/`: modified xv6-riscv kernel, user programs, and test tools.
- `docs/`: Docusaurus documentation site for the assignment phases and compliance context.
- `screenshots/`: local evidence folder for boot/test screenshots.

## Quick Start

Install the required toolchain on Fedora:

```bash
sudo dnf install make gcc perl python3 bc qemu-system-riscv-core gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu nodejs npm
```

Build xv6:

```bash
cd xv6-security
make clean
make
```

Run xv6 in the terminal:

```bash
make qemu-nox
```

Use these demo accounts at the secure login prompt:

| User | Password | Role |
| --- | --- | --- |
| `admin` | `admin123` | administrator |
| `patient1` | `patient123` | patient |
| `doctor1` | `doctor123` | clinician |

After logging in as `admin`, run:

```sh
compliance_test
audit_dump
```

The compliance runner should report `Passed: 18 / 18`.

## Documentation

Build the Docusaurus documentation locally:

```bash
cd docs
npm install
npm run build
```

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
