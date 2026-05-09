---
sidebar_position: 7
title: FDA and IEC 62443 Context
---

# FDA and IEC 62443 Context

## What and Why

The project is a teaching model, but its controls map to real medical-device and industrial-control security ideas. FDA cybersecurity guidance and IEC 62443 both emphasize controlled access, least privilege, traceability, and secure maintenance.

The xv6 implementation is intentionally small, but it demonstrates the operating-system foundations behind those expectations.

## Theory

Medical devices are safety-relevant systems. Security controls should reduce the chance that a user, process, or attacker can change therapy data, alter configuration, or hide activity.

IEC 62443 uses concepts such as zones, conduits, least privilege, auditability, and role-based access. FDA medical-device cybersecurity guidance emphasizes secure design, authentication, authorization, logging, update planning, and vulnerability management across the device lifecycle.

## Implementation Walk-through

This project maps the controls as follows:

| Project Feature | Security Meaning | Standards Context |
| --- | --- | --- |
| Secure login | User identification and authentication | Access control and accountable use |
| Roles and uid/gid | Least privilege | Role-based permissions |
| File modes | Data and configuration protection | Authorization and separation of duties |
| Protected medical files | Realistic assets | Patient safety and device integrity |
| Audit ring | Traceability | Logging, monitoring, forensic support |
| Compliance test | Repeatable evidence | Verification and validation |

The design is intentionally transparent. Students can inspect how a syscall moves from user code into the kernel and how the kernel makes a security decision.

## How to Test

Use the compliance report as evidence of the implemented controls:

```sh
compliance_test
```

Then run:

```sh
audit_dump
```

The audit output demonstrates that the system records attempted and completed operations with uid and syscall result data.

For documentation evidence, build the site:

```bash
cd docs
npm install
npm run build
```

## Common Pitfalls

Do not claim this xv6 project is a certified medical-device operating system. It is a teaching implementation that models security ideas in a minimal kernel.

Do not confuse authentication with authorization. Login identifies the user, but file permissions and syscall authorization decide what that user can do.

Do not treat audit logs as prevention. Audit data helps detection and investigation; it must be paired with access control and secure defaults.
