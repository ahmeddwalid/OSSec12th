#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static int
octal(char *s)
{
  int v = 0;

  while(*s >= '0' && *s <= '7'){
    v = v * 8 + (*s - '0');
    s++;
  }
  return v;
}

int
main(int argc, char *argv[])
{
  if(argc != 3){
    fprintf(2, "usage: chmod mode path\n");
    exit(1);
  }
  if(chmod(argv[2], octal(argv[1])) < 0){
    fprintf(2, "chmod: failed\n");
    exit(1);
  }
  exit(0);
}