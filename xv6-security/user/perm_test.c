#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

static int pass_count;
static int fail_count;

static void
result(char *label, int ok)
{
  if(ok){
    printf("[PASS] %s\n", label);
    pass_count++;
  } else {
    printf("[FAIL] %s\n", label);
    fail_count++;
  }
}

static void
check_open_denied(char *label, char *user, char *pw, char *path, int mode)
{
  int fd;
  login(user, pw);
  fd = open(path, mode);
  result(label, fd < 0);
  if(fd >= 0)
    close(fd);
}

static void
check_open_ok(char *label, char *user, char *pw, char *path, int mode)
{
  int fd;
  login(user, pw);
  fd = open(path, mode);
  result(label, fd >= 0);
  if(fd >= 0)
    close(fd);
}

int
main(void)
{
  int fd;

  check_open_denied("patient denied /device/config", "patient1", "patient123", "/device/config", O_RDONLY);

  login("doctor1", "doctor123");
  fd = open("/dosage/insulin.log", O_WRONLY);
  if(fd >= 0){
    result("doctor writes insulin log", write(fd, "dose=4\n", 7) == 7);
    close(fd);
  } else {
    result("doctor writes insulin log", 0);
  }

  check_open_ok("admin opens /device/config", "admin", "admin123", "/device/config", O_RDONLY);

  printf("perm_test: %d passed, %d failed\n", pass_count, fail_count);
  exit(fail_count > 0 ? 1 : 0);
}