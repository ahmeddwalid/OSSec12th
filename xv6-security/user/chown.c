#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){
    fprintf(2, "usage: chown uid gid path\n");
    exit(1);
  }
  if(chown(argv[3], atoi(argv[1]), atoi(argv[2])) < 0){
    fprintf(2, "chown: failed\n");
    exit(1);
  }
  exit(0);
}