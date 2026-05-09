#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(void)
{
  char buf[96];

  if(whoami(buf, sizeof(buf)) < 0){
    fprintf(2, "whoami: not authenticated\n");
    exit(1);
  }
  printf("%s", buf);
  exit(0);
}