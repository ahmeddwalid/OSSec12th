---
sidebar_position: 7
title: FDA & IEC 62443 Context
---

# Regulatory Context: FDA & IEC 62443

:::warning Disclaimer
This project is a **teaching model** built on a simplified OS kernel. It does not claim compliance with any regulatory standard. The mappings below are educational illustrations of how real-world security requirements relate to the implementation concepts used here.
:::

## The Medtronic MiniMed 508 Story

In 2019 the FDA issued a **Class I recall** (its most serious category) for the Medtronic MiniMed 508 insulin pump. Security researchers demonstrated that an attacker within radio range could wirelessly send commands to the pump, altering insulin doses without authentication.

Key facts:
- **CVE:** [CVE-2019-10964](https://nvd.nist.gov/vuln/detail/CVE-2019-10964)
- **Device:** Medtronic MiniMed 508 and Paradigm series
- **Vulnerability:** Cleartext wireless command protocol, no authentication
- **Consequence:** Potential lethal insulin overdose or underdose
- **FDA action:** Class I recall (risk of serious injury or death)
- **Affected units:** ~4,000 devices in active use

This incident drove two regulatory developments that shape modern medical-device cybersecurity:

1. **FDA 2023 Cybersecurity Guidance**: pre-market requirements for new devices
2. Renewed emphasis on **IEC 62443** as the referenced industrial security standard

## IEC 62443 at a Glance

IEC 62443 is the international standard for **Industrial Automation and Control System (IACS) cybersecurity**. Medical devices fall under its scope when they communicate with external systems or are embedded in clinical infrastructure.

### Key Concepts

**Security Zones and Conduits**
A zone is a logical grouping of assets that share the same security policy. A conduit is a communication path between zones. The standard requires that zone boundaries be enforced and that conduits be protected (authenticated, encrypted).

*In this project:* the kernel privilege boundary between `uid=0` (admin zone) and `uid>0` (patient/clinician zone) maps to the zone concept. The ECALL interface is the conduit: authenticated by the `proc->authenticated` flag and the `uid` check.

**Security Levels (SL 1–4)**
- SL 1: Protection against unintentional misuse
- SL 2: Protection against intentional violation using simple means
- SL 3: Protection against intentional violation using sophisticated means
- SL 4: Protection against state-level attacks

*In this project:* the implementation targets SL 1–2: it prevents accidental and simple intentional misuse, but is not hardened against sophisticated adversaries.

**Least Privilege**
Each process and user should operate with the minimum privilege needed for its function.

*In this project:* A patient process cannot call `useradd`, read `/device/config`, or read the audit ring: enforced kernel-side.

**Auditability**
The system must record security-relevant events so that incidents can be reconstructed.

*In this project:* Phase 3 ring buffer + trap audit printing.

## FDA 2023 Cybersecurity Guidance

The FDA's 2023 guidance document *"Cybersecurity in Medical Devices: Quality System Considerations and Content of Premarket Submissions"* lists these pre-market requirements:

| FDA Requirement | Description |
|-----------------|-------------|
| Identify and protect | Identify cybersecurity risks and implement protections |
| Authentication | Authentication for privileged access to device functions |
| Authorization | Role-based access control to device resources |
| Cryptographic integrity | Verify software integrity at load and update time |
| Audit logging | Maintain logs of security-relevant events |
| Updateability | Support security patches without unsafe downtime |
| Coordinated disclosure | Maintain a SBOM and a vulnerability disclosure policy |

## Requirements Mapping Table

| Regulatory Requirement | IEC 62443 Clause | xv6 Implementation |
|-----------------------|-----------------|-------------------|
| User authentication before access | IAC, SR 1.1 | Phase 1: `sys_login`, `/etc/passwd`, `proc->authenticated` |
| Role-based access control | SR 2.1 | Phase 1: `proc->role` (admin/clinician/patient) |
| Least-privilege file access | SR 2.1, SR 2.2 | Phase 2: inode `mode`/`uid`/`gid`, `perm_check()` |
| Audit logging of security events | SR 2.8 | Phase 3: ring buffer, `audit_log()`, trap print |
| Audit accessible only to authorised users | SR 2.8, SR 2.9 | Phase 3: `audit_read` returns `EPERM` for non-admin |
| Device configuration protected | SR 2.1 | `/device/config` mode `0600`, owner uid=0 |

## Glossary

| Term | Meaning |
|------|---------|
| **UID** | User ID: a numeric identity assigned to an OS user. UID 0 is the superuser. |
| **GID** | Group ID: a numeric identity for a group of users sharing permissions. |
| **DAC** | Discretionary Access Control: permissions controlled by resource owners (Unix mode bits). |
| **MAC** | Mandatory Access Control: system-wide policy overrides owner preferences (SELinux, etc.). |
| **Audit trail** | A chronological record of security-relevant events for forensic reconstruction. |
| **Ring buffer** | A fixed-size circular array; oldest entries are overwritten when full. |
| **ECALL** | RISC-V instruction that transfers control from user mode to the OS kernel. |
| **RISC-V** | Reduced Instruction Set Computer: V; an open ISA used as xv6's target architecture. |
| **SMP** | Symmetric Multi-Processing: multiple CPU harts sharing memory. |
| **virtio** | A paravirtualized I/O standard used by QEMU for disk and network. |
| **IEC 62443** | International standard for industrial and embedded system cybersecurity. |
| **FDA guidance** | Non-binding but expected recommendations from the U.S. Food & Drug Administration. |
| **Class I recall** | FDA's most serious recall class: risk of serious injury or death. |
| **SL** | Security Level (IEC 62443): scale 1–4 of adversary sophistication targeted. |
| **SBOM** | Software Bill of Materials: inventory of all software components in a device. |
| **EPERM** | POSIX error code: Operation not permitted. Returned for unauthorized syscalls. |
