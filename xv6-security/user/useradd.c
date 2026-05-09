#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 4){
    fprintf(2, "usage: useradd username password role\n");
    exit(1);
  }
  if(useradd(argv[1], argv[2], atoi(argv[3])) < 0){
    fprintf(2, "useradd: failed\n");
    exit(1);
  }
  exit(0);
}