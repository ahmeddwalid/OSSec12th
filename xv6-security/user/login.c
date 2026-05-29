#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// reads one line from fd 0 (console). handles backspace (0x7f/DEL) for raw
// terminal editing — xv6 has no cooked mode or readline library.
static void
read_line(char *buf, int max)
{
  int i = 0;
  char c;

  while(i < max - 1){
    if(read(0, &c, 1) != 1)
      break;
    if(c == '\n' || c == '\r')
      break;
    if(c == '\b' || c == 0x7f){
      if(i > 0)
        i--;
      continue;
    }
    buf[i++] = c;
  }
  buf[i] = 0;
}

int
main(void)
{
  char username[16];
  char password[64];
  int failures = 0;
  char *argv[] = { "sh", 0 };

  printf("xv6 Medical Device OS - Secure Login\n");
  for(;;){
    printf("Username: ");
    read_line(username, sizeof(username));
    printf("Password: ");
    read_line(password, sizeof(password));

    if(login(username, password) == 0){
      exec("sh", argv);   // replace login with shell — discards this process
      printf("login: exec sh failed\n");
      exit(1);
    }

    failures++;
    printf("Login failed.\n");
    if(failures >= 3){
      // deliberate lockout: no reset, device must power-cycle. mimics
      // medical device security best practices for brute-force prevention.
      printf("Device locked after 3 failed attempts.\n");
      for(;;)
        pause(1000);
    }
  }
}