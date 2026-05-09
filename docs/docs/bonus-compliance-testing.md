---
sidebar_position: 6
title: Bonus Compliance Testing
---

# Bonus: Compliance Testing

## Purpose

The `compliance_test` program is an automated end-to-end test suite that exercises all three security phases in a single run. It was designed for three audiences:

1. **Graders**: produces a structured `PASS / FAIL` report with student identity
2. **Developers**: catches regressions after kernel changes
3. **Auditors**: maps each test to a security requirement

## How Tests Work

Each test follows this pattern:

```c
static void assert_pass(const char *name, int cond) {
  passes += cond;
  fails  += !cond;
  printf("[%s] %s\n", cond ? "PASS" : "FAIL", name);
}

static void assert_fail(const char *name, int cond) {
  assert_pass(name, !cond);   // we expect the operation to be denied
}
```

`assert_fail` is used for "this must be rejected" tests. If the kernel incorrectly allows the operation, the test reports `FAIL`.

## The 18 Tests

### Phase 1: Authentication (T01–T06)

| ID | Test name | Description |
|----|-----------|-------------|
| T01 | `valid_admin_login` | `login("root","root123")` → 0 |
| T02 | `valid_patient_login` | `login("patient1","patient123")` → 0 |
| T03 | `wrong_password_rejected` | `login("root","wrong")` → -1 |
| T04 | `whoami_returns_username` | `whoami(buf,16)` returns "patient1" |
| T05 | `useradd_by_patient_denied` | `useradd(...)` as patient → -1 |
| T06 | `userdel_by_patient_denied` | `userdel(...)` as patient → -1 |

### Phase 2: File Permissions (T07–T12)

| ID | Test name | Description |
|----|-----------|-------------|
| T07 | `patient_cannot_open_config` | `open("/device/config", O_RDONLY)` as patient → -1 |
| T08 | `patient_can_read_records` | `open("/patient/records", O_RDONLY)` as patient → fd ≥ 0 |
| T09 | `patient_cannot_write_records` | `open("/patient/records", O_WRONLY)` as patient → -1 |
| T10 | `doctor_can_write_dosage` | `open("/dosage/insulin.log", O_WRONLY)` as doctor → fd ≥ 0 |
| T11 | `doctor_cannot_read_config` | `open("/device/config", O_RDONLY)` as doctor → -1 |
| T12 | `admin_can_open_all` | Admin opens `/device/config`, `/audit/syscall.log` → success |

### Phase 3: Audit Log (T13–T17)

| ID | Test name | Description |
|----|-----------|-------------|
| T13 | `patient_cannot_read_audit` | `audit_read(buf, n)` as patient → -1 |
| T14 | `admin_can_read_audit` | `audit_read(buf, n)` as admin → entries returned |
| T15 | `audit_records_denied_open` | T07 denial appears in audit ring |
| T16 | `audit_records_successful_write` | T10 success appears in audit ring |
| T17 | `end_to_end_attack_detection` | Login→deny→audit confirms the attacker attempt |

### Integration (T18)

| ID | Test name | Description |
|----|-----------|-------------|
| T18 | `all_phases_active` | Re-verify auth + permission + audit all active simultaneously |

## T17 End-to-End Narrative

T17 is the flagship test. It simulates a realistic attack scenario:

1. **Authenticate as patient** (`login("patient1","patient123")`)
2. **Attempt unauthorized access** (`open("/device/config", O_RDONLY)` → denied)
3. **Switch to admin** (`login("root","root123")`)
4. **Read audit ring** (`audit_read(buf, sizeof buf)`)
5. **Search ring for the denial**: find an entry with `syscall_no == SYS_open`, `uid == <patient_uid>`, `result == -1`
6. **Assert found**: if the entry exists, T17 `PASS`

This proves all three phases interlock: Phase 1 provided the UID, Phase 2 denied the access and returned -1, Phase 3 recorded the result.

![QEMU terminal showing compliance_test output: 18/18 PASS](/img/screenshots/compliance-full-pass.png)

## Running the Tests

```bash
# Build and boot
cd xv6-security
make clean && make
make qemu

# At the login prompt
login: root
password: root123

# Run the suite
$ compliance_test
```

## Known Implementation Details

- **`DIRSIZ 16`**: The filename `compliance_test` is 16 characters, requiring `DIRSIZ` increased from 14 to 16 in `kernel/fs.h`. `mkfs.c` padding was updated accordingly.
- **Audit ring ordering**: T15/T16 search backward from the newest entry. The ring may contain stale entries from earlier test phases; the search uses `uid` + `result` as a compound key.
- **No libc**: xv6 user programs have no C standard library. The test uses `memcmp` for string comparison and manual `itoa` for number formatting.

