#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/audit.h"
#include "user/user.h"

static int pass_count;
static int fail_count;
static struct audit_entry audit_entries[AUDIT_BUF_SIZE];

#define ASSERT_EQ(label, got, expected) do { \
  int _got = (got); \
  int _expected = (expected); \
  if(_got == _expected){ \
    printf("[PASS] %s\n", label); \
    pass_count++; \
  } else { \
    printf("[FAIL] %s - got %d, expected %d\n", label, _got, _expected); \
    fail_count++; \
  } \
} while(0)

#define ASSERT_EPERM(label, ret) ASSERT_EQ(label, ret, -1)
#define ASSERT_OK(label, ret) do { \
  int _ret = (ret); \
  if(_ret >= 0){ \
    printf("[PASS] %s\n", label); \
    pass_count++; \
  } else { \
    printf("[FAIL] %s - errno %d\n", label, _ret); \
    fail_count++; \
  } \
} while(0)

static int
contains(char *haystack, char *needle)
{
  int n = strlen(needle);

  for(int i = 0; haystack[i]; i++){
    if(memcmp(haystack + i, needle, n) == 0)
      return 1;
  }
  return 0;
}

static int
open_close(char *path, int mode)
{
  int fd = open(path, mode);

  if(fd >= 0)
    close(fd);
  return fd;
}

static int
read_ok(char *path)
{
  char buf[64];
  int fd = open(path, O_RDONLY);
  int n;

  if(fd < 0)
    return -1;
  n = read(fd, buf, sizeof(buf));
  close(fd);
  return n;
}

static int
write_text(char *path, char *text)
{
  int fd = open(path, O_WRONLY);
  int n;

  if(fd < 0)
    return -1;
  n = write(fd, text, strlen(text));
  close(fd);
  return n;
}

static int
load_audit_as_admin(void)
{
  int n;

  login("admin", "admin123");
  memset(audit_entries, 0, sizeof(audit_entries));
  n = audit_read((char*)audit_entries, sizeof(audit_entries));
  if(n < 0)
    return -1;
  return n / sizeof(struct audit_entry);
}

static int
audit_has(int syscall_no, int result, int exact)
{
  int count = load_audit_as_admin();

  if(count < 0)
    return 0;
  for(int i = 0; i < count; i++){
    if(audit_entries[i].syscall_no == syscall_no){
      if(exact && audit_entries[i].result == result)
        return 1;
      if(!exact && audit_entries[i].result >= result)
        return 1;
    }
  }
  return 0;
}

int
main(void)
{
  char who[96];
  int fd;
  int denial_ret;
  int attack_ret;
  int phases_ok;

  ASSERT_OK("T01 valid admin login succeeds", login("admin", "admin123"));
  ASSERT_OK("T02 valid patient login succeeds", login("patient1", "patient123"));
  ASSERT_OK("T03 valid doctor login succeeds", login("doctor1", "doctor123"));
  ASSERT_EPERM("T04 wrong password is rejected", login("doctor1", "wrong"));

  login("patient1", "patient123");
  ASSERT_EPERM("T05 non-admin cannot call useradd", useradd("bad", "bad123", 1));

  login("doctor1", "doctor123");
  memset(who, 0, sizeof(who));
  ASSERT_EQ("T06 whoami returns correct username", whoami(who, sizeof(who)) >= 0 && contains(who, "doctor1"), 1);

  login("patient1", "patient123");
  ASSERT_EPERM("T07 patient cannot open /device/config", open_close("/device/config", O_RDONLY));
  ASSERT_OK("T08 patient can read /patient/records", read_ok("/patient/records"));
  ASSERT_EPERM("T09 patient cannot write /patient/records", open_close("/patient/records", O_WRONLY));

  login("doctor1", "doctor123");
  ASSERT_OK("T10 doctor can write /dosage/insulin.log", write_text("/dosage/insulin.log", "dose=4\n"));
  ASSERT_EPERM("T11 doctor cannot read /device/config", open_close("/device/config", O_RDONLY));

  login("admin", "admin123");
  fd = open("/device/config", O_RDONLY);
  phases_ok = fd >= 0;
  if(fd >= 0) close(fd);
  fd = open("/patient/records", O_RDONLY);
  phases_ok = phases_ok && fd >= 0;
  if(fd >= 0) close(fd);
  fd = open("/dosage/insulin.log", O_RDWR);
  phases_ok = phases_ok && fd >= 0;
  if(fd >= 0) close(fd);
  ASSERT_EQ("T12 admin can open all protected files", phases_ok, 1);

  login("patient1", "patient123");
  ASSERT_EPERM("T13 audit_read by non-admin returns EPERM", audit_read((char*)audit_entries, sizeof(audit_entries)));

  ASSERT_EQ("T14 audit_read by admin returns data", load_audit_as_admin() > 0, 1);
  login("patient1", "patient123");
  denial_ret = open_close("/device/config", O_RDONLY);
  ASSERT_EQ("T15 log contains EPERM denial event", denial_ret == -1 && audit_has(SYS_open, -1, 1), 1);
  ASSERT_EQ("T16 log contains successful write event", audit_has(SYS_write, 1, 0), 1);

  login("patient1", "patient123");
  attack_ret = open_close("/device/config", O_RDONLY);
  ASSERT_EQ("T17 attack denied and detected in audit", attack_ret == -1 && audit_has(SYS_open, -1, 1), 1);

  phases_ok = login("admin", "admin123") == 0 &&
              open_close("/device/config", O_RDONLY) >= 0 &&
              load_audit_as_admin() > 0;
  ASSERT_EQ("T18 all three phases active simultaneously", phases_ok, 1);

  printf("\n==========================================\n");
  printf("  COMPLIANCE REPORT - CCY4304 12th Project\n");
  printf("  Students: Ahmed Walid - 221011183\n");
  printf("            Ahmed Mohamed Mahmoud - 221010720\n");
  printf("  Passed: %d / %d\n", pass_count, pass_count + fail_count);
  printf("==========================================\n");
  exit(fail_count > 0 ? 1 : 0);
}