---
sidebar_position: 2
title: Environment Setup
---

# Environment Setup

## What and Why

The project needs a RISC-V cross compiler, binutils, QEMU, Make, Perl, and Node.js. xv6 builds a RISC-V kernel and filesystem image, then boots them in QEMU. Docusaurus builds the documentation site.

A predictable environment matters because small toolchain differences can look like kernel bugs. Modern binutils, for example, warn about xv6 linker segments unless the Makefile suppresses expected RWX segment warnings.

## Theory

xv6 is cross-compiled. The host machine runs Linux on the developer workstation, but the output kernel targets RISC-V. QEMU provides a virtual RISC-V board with virtio disk support. The Makefile detects a working RISC-V tool prefix and then builds the kernel, user programs, `mkfs`, and `fs.img`.

This project was validated with the `riscv64-linux-gnu-` toolchain and `qemu-system-riscv64`.

## Implementation Walk-through

On Fedora, install the system packages:

```bash
sudo dnf install make gcc perl python3 bc qemu-system-riscv-core gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu nodejs npm
```

Confirm the tools:

```bash
riscv64-linux-gnu-gcc --version
qemu-system-riscv64 --version
node --version
npm --version
```

Build xv6:

```bash
cd xv6-security
make clean
make
```

Build the docs:

```bash
cd docs
npm install
npm run build
```

## How to Test

A clean xv6 build should produce no compiler warnings or errors:

```bash
cd xv6-security
make clean
make 2>&1 | grep -E "error:|warning:"
```

No output from the grep command means the quality gate passed.

Boot xv6 with:

```bash
make qemu-nox
```

The target reuses xv6's nographic QEMU mode, so the login prompt appears directly in the terminal.

## Common Pitfalls

Some xv6 guides mention `make qemu-nox`, while this xv6 snapshot originally provided only `make qemu`. This project adds `qemu-nox` as a compatibility alias.

Package names differ by distribution. Fedora uses `binutils-riscv64-linux-gnu`; other distributions may package the same tools under a different name.

Do not commit generated build artifacts such as `fs.img`, `kernel/kernel`, object files, or generated user binaries. They are ignored and should be rebuilt locally.
