#ifndef AUTH_H
#define AUTH_H

#define ROLE_ADMIN   0
#define ROLE_PATIENT 1
#define ROLE_DOCTOR  2

#define MAX_USERS    16
#define PASSWD_FILE  "/etc/passwd"
#define HASH_LEN     32

struct credential {
  char username[16];
  int uid;
  int gid;
  int role;
  char hash[HASH_LEN + 1];
};

int  auth_login(char *username, char *password);
int  auth_useradd(char *username, char *password, int role);
int  auth_userdel(char *username);
int  auth_passwd(char *username, char *old_pw, char *new_pw);
int  auth_whoami(char *buf, int bufsz);
void auth_init(void);
void pw_hash(const char *password, char *out_hex);

#endif