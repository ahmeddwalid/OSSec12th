#ifndef PERMS_H
#define PERMS_H

#define PERM_OWNER_READ  0400
#define PERM_OWNER_WRITE 0200
#define PERM_OWNER_EXEC  0100
#define PERM_GROUP_READ  0040
#define PERM_GROUP_WRITE 0020
#define PERM_GROUP_EXEC  0010
#define PERM_OTHER_READ  0004
#define PERM_OTHER_WRITE 0002
#define PERM_OTHER_EXEC  0001

struct inode;

int perm_check(struct inode *ip, char access);

#endif