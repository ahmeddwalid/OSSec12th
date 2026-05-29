#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(2, "usage: userdel username\n");
    exit(1);
  }
  // admin-only; cannot delete the 'admin' account — both enforced by kernel
  if(userdel(argv[1]) < 0){
    fprintf(2, "userdel: failed\n");
    exit(1);
  }
  exit(0);
}