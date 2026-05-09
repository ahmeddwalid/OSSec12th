#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "proc.h"
#include "auth.h"

#define PASSWD_BUF_SIZE 1024
#define MAX_PASSWORD    64

static int
streq(const char *a, const char *b)
{
  return strlen(a) == strlen(b) && strncmp(a, b, strlen(a)) == 0;
}

static int
emptystr(const char *s)
{
  return s == 0 || s[0] == 0;
}

static void
append_char(char *buf, int *off, int max, char c)
{
  if(*off < max - 1)
    buf[(*off)++] = c;
  buf[*off] = 0;
}

static void
append_str(char *buf, int *off, int max, const char *s)
{
  while(*s)
    append_char(buf, off, max, *s++);
}

static void
append_int(char *buf, int *off, int max, int v)
{
  char tmp[16];
  int i = 0;

  if(v == 0){
    append_char(buf, off, max, '0');
    return;
  }
  if(v < 0){
    append_char(buf, off, max, '-');
    v = -v;
  }
  while(v > 0 && i < (int)sizeof(tmp)){
    tmp[i++] = '0' + (v % 10);
    v /= 10;
  }
  while(i > 0)
    append_char(buf, off, max, tmp[--i]);
}

static int
parse_int(char **cursor, int *out)
{
  int v = 0;
  char *p = *cursor;

  if(*p < '0' || *p > '9')
    return -1;
  while(*p >= '0' && *p <= '9'){
    v = v * 10 + (*p - '0');
    p++;
  }
  *out = v;
  *cursor = p;
  return 0;
}

static int
parse_field(char **cursor, char *out, int outsz, char delim)
{
  int n = 0;
  char *p = *cursor;

  while(*p && *p != delim && *p != '\n'){
    if(n < outsz - 1)
      out[n++] = *p;
    p++;
  }
  out[n] = 0;
  if(*p != delim)
    return -1;
  *cursor = p + 1;
  return 0;
}

static int
parse_credentials(char *data, struct credential creds[], int max)
{
  int count = 0;
  char *p = data;

  while(*p && count < max){
    if(*p == '\n'){
      p++;
      continue;
    }
    if(parse_field(&p, creds[count].username, sizeof(creds[count].username), '|') < 0)
      return -1;
    if(parse_int(&p, &creds[count].uid) < 0 || *p++ != '|')
      return -1;
    if(parse_int(&p, &creds[count].gid) < 0 || *p++ != '|')
      return -1;
    if(parse_int(&p, &creds[count].role) < 0 || *p++ != '|')
      return -1;
    if(parse_field(&p, creds[count].hash, sizeof(creds[count].hash), '\n') < 0){
      int n = 0;
      while(*p && n < HASH_LEN){
        creds[count].hash[n++] = *p++;
      }
      creds[count].hash[n] = 0;
      if(*p != 0)
        return -1;
    }
    count++;
  }
  return count;
}

static void
append_credential(char *buf, int *off, int max, struct credential *c)
{
  append_str(buf, off, max, c->username);
  append_char(buf, off, max, '|');
  append_int(buf, off, max, c->uid);
  append_char(buf, off, max, '|');
  append_int(buf, off, max, c->gid);
  append_char(buf, off, max, '|');
  append_int(buf, off, max, c->role);
  append_char(buf, off, max, '|');
  append_str(buf, off, max, c->hash);
  append_char(buf, off, max, '\n');
}

static struct inode*
auth_create(char *path, short type)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);
  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    return ip;
  }
  if((ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }
  ilock(ip);
  ip->nlink = 1;
  iupdate(ip);
  if(type == T_DIR){
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      goto fail;
  }
  if(dirlink(dp, name, ip->inum) < 0)
    goto fail;
  if(type == T_DIR){
    dp->nlink++;
    iupdate(dp);
  }
  iunlockput(dp);
  return ip;

fail:
  ip->nlink = 0;
  iupdate(ip);
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

static int
read_passwd(struct credential creds[], int max)
{
  char buf[PASSWD_BUF_SIZE];
  struct inode *ip;
  int n;

  begin_op();
  if((ip = namei(PASSWD_FILE)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  n = readi(ip, 0, (uint64)buf, 0, sizeof(buf) - 1);
  iunlockput(ip);
  end_op();
  if(n < 0)
    return -1;
  buf[n] = 0;
  return parse_credentials(buf, creds, max);
}

static int
write_passwd(struct credential creds[], int count)
{
  char buf[PASSWD_BUF_SIZE];
  int off = 0;
  struct inode *ip;
  int wrote;

  memset(buf, 0, sizeof(buf));
  for(int i = 0; i < count; i++)
    append_credential(buf, &off, sizeof(buf), &creds[i]);
  if(off >= sizeof(buf) - 1)
    return -1;

  begin_op();
  if((ip = namei(PASSWD_FILE)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  itrunc(ip);
  wrote = writei(ip, 0, (uint64)buf, 0, off);
  iunlockput(ip);
  end_op();
  return wrote == off ? 0 : -1;
}

static void
seed_credential(struct credential *c, char *username, int uid, int role, char *password)
{
  safestrcpy(c->username, username, sizeof(c->username));
  c->uid = uid;
  c->gid = uid;
  c->role = role;
  pw_hash(password, c->hash);
}

void
pw_hash(const char *password, char *out_hex)
{
  static char hex[] = "0123456789abcdef";
  uint h[4] = { 5381, 52711, 33013, 87911 };

  // This teaching kernel uses a deterministic djb2-style hash so it can run
  // without a crypto library. Production password storage should use bcrypt
  // or Argon2 with per-user salts and a tunable work factor.
  for(; *password; password++){
    uchar c = *password;
    h[0] = ((h[0] << 5) + h[0]) ^ c;
    h[1] = ((h[1] << 5) + h[1]) + c + h[0];
    h[2] = ((h[2] << 5) + h[2]) ^ (c << 1);
    h[3] = ((h[3] << 5) + h[3]) + (c ^ h[2]);
  }
  for(int i = 0; i < 4; i++){
    for(int shift = 28; shift >= 0; shift -= 4)
      *out_hex++ = hex[(h[i] >> shift) & 0xf];
  }
  *out_hex = 0;
}

void
auth_init(void)
{
  struct inode *ip;
  struct credential creds[3];

  begin_op();
  if((ip = namei("/etc")) == 0){
    if((ip = auth_create("/etc", T_DIR)) == 0)
      panic("auth_init: /etc");
    iunlockput(ip);
  } else {
    iput(ip);
  }
  end_op();

  begin_op();
  if((ip = namei(PASSWD_FILE)) == 0){
    if((ip = auth_create(PASSWD_FILE, T_FILE)) == 0)
      panic("auth_init: passwd");
    iunlockput(ip);
    end_op();
    seed_credential(&creds[0], "admin", 0, ROLE_ADMIN, "admin123");
    seed_credential(&creds[1], "patient1", 1, ROLE_PATIENT, "patient123");
    seed_credential(&creds[2], "doctor1", 2, ROLE_DOCTOR, "doctor123");
    if(write_passwd(creds, 3) < 0)
      panic("auth_init: seed");
    return;
  }
  iput(ip);
  end_op();
}

int
auth_login(char *username, char *password)
{
  struct credential creds[MAX_USERS];
  char hash[HASH_LEN + 1];
  struct proc *p = myproc();
  int count = read_passwd(creds, MAX_USERS);

  if(count < 0 || emptystr(username) || emptystr(password))
    return -1;
  pw_hash(password, hash);
  for(int i = 0; i < count; i++){
    if(streq(username, creds[i].username) && streq(hash, creds[i].hash)){
      p->uid = creds[i].uid;
      p->gid = creds[i].gid;
      p->role = creds[i].role;
      safestrcpy(p->username, creds[i].username, sizeof(p->username));
      p->authenticated = 1;
      return 0;
    }
  }
  return -1;
}

int
auth_useradd(char *username, char *password, int role)
{
  struct proc *p = myproc();
  struct credential creds[MAX_USERS];
  int count;

  if(p == 0 || p->uid != 0 || !p->authenticated)
    return -1;
  if(emptystr(username) || emptystr(password) || role < ROLE_ADMIN || role > ROLE_DOCTOR)
    return -1;
  count = read_passwd(creds, MAX_USERS);
  if(count < 0 || count >= MAX_USERS)
    return -1;
  for(int i = 0; i < count; i++){
    if(streq(username, creds[i].username))
      return -1;
  }
  safestrcpy(creds[count].username, username, sizeof(creds[count].username));
  creds[count].uid = role;
  creds[count].gid = role;
  creds[count].role = role;
  pw_hash(password, creds[count].hash);
  return write_passwd(creds, count + 1);
}

int
auth_userdel(char *username)
{
  struct proc *p = myproc();
  struct credential creds[MAX_USERS];
  int count, out = 0, found = 0;

  if(p == 0 || p->uid != 0 || !p->authenticated || emptystr(username))
    return -1;
  if(streq(username, "admin"))
    return -1;
  count = read_passwd(creds, MAX_USERS);
  if(count < 0)
    return -1;
  for(int i = 0; i < count; i++){
    if(streq(username, creds[i].username)){
      found = 1;
      continue;
    }
    if(out != i)
      creds[out] = creds[i];
    out++;
  }
  if(!found)
    return -1;
  return write_passwd(creds, out);
}

int
auth_passwd(char *username, char *old_pw, char *new_pw)
{
  struct proc *p = myproc();
  struct credential creds[MAX_USERS];
  char old_hash[HASH_LEN + 1];
  int count;

  if(p == 0 || !p->authenticated || emptystr(username) || emptystr(new_pw))
    return -1;
  if(p->uid != 0 && !streq(username, p->username))
    return -1;
  count = read_passwd(creds, MAX_USERS);
  if(count < 0)
    return -1;
  pw_hash(old_pw, old_hash);
  for(int i = 0; i < count; i++){
    if(streq(username, creds[i].username)){
      if(p->uid != 0 && !streq(old_hash, creds[i].hash))
        return -1;
      pw_hash(new_pw, creds[i].hash);
      return write_passwd(creds, count);
    }
  }
  return -1;
}

int
auth_whoami(char *buf, int bufsz)
{
  struct proc *p = myproc();
  int off = 0;

  if(p == 0 || !p->authenticated || bufsz <= 0)
    return -1;
  append_str(buf, &off, bufsz, p->username);
  append_str(buf, &off, bufsz, " uid=");
  append_int(buf, &off, bufsz, p->uid);
  append_str(buf, &off, bufsz, " gid=");
  append_int(buf, &off, bufsz, p->gid);
  append_str(buf, &off, bufsz, " role=");
  append_int(buf, &off, bufsz, p->role);
  append_char(buf, &off, bufsz, '\n');
  return off;
}