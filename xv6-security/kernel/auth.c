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

#define PASSWD_BUF_SIZE 2048 // enough for MAX_USERS (16) pipe-delimited credentials
#define MAX_PASSWORD    64

// constant-time-safe: checks length before comparing bytes
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

// writes decimal integer into buffer. tmp[16] holds "-2147483648" + nul
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
  static char buf[PASSWD_BUF_SIZE]; // kept off the 1-page kernel stack
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
  static char buf[PASSWD_BUF_SIZE]; // kept off the 1-page kernel stack
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

// sha-256 per FIPS 180-4. runs in kernel context — no malloc, only stack vars.
// outputs 64 hex chars + nul into out_hex.
void
pw_hash(const char *password, char *out_hex)
{
  static const char hex[] = "0123456789abcdef";
  static const uint K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
  };

  uint H[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
  };

  uchar block[128];
  uint W[64];
  uint a, b, c, d, e, f, g, h, t1, t2, S0, S1, ch, maj;
  uint msglen, i, j, blocks;
  uint64 bitlen;

  msglen = strlen((char*)password);
  bitlen = (uint64)msglen * 8;

  for(i = 0; i < msglen; i++)
    block[i] = password[i];

    // sha-256 padding per FIPS 180-4: 0x80, zero-fill, 64-bit big-endian bit length.
  // single block if msg < 56 bytes, two blocks if 56–63 bytes.
  block[msglen] = 0x80;

  if(msglen < 56){
    for(i = msglen + 1; i < 56; i++)
      block[i] = 0;
    blocks = 1;
  } else {
    for(i = msglen + 1; i < 64; i++)
      block[i] = 0;
    j = (msglen >= 64) ? 1 : 0;
    for(i = j; i < 56; i++)
      block[64 + i] = 0;
    blocks = 2;
  }

  j = blocks * 64 - 8;
  for(i = 0; i < 8; i++)
    block[j++] = (bitlen >> (56 - i * 8)) & 0xff;

  for(j = 0; j < blocks; j++){
    for(i = 0; i < 16; i++){
      uint off = j * 64 + i * 4;
      W[i] = ((uint)block[off] << 24) | ((uint)block[off+1] << 16) |
             ((uint)block[off+2] << 8) | (uint)block[off+3];
    }

    for(i = 16; i < 64; i++){
      S0 = ((W[i-15] >> 7) | (W[i-15] << 25)) ^
           ((W[i-15] >> 18) | (W[i-15] << 14)) ^ (W[i-15] >> 3);
      S1 = ((W[i-2] >> 17) | (W[i-2] << 15)) ^
           ((W[i-2] >> 19) | (W[i-2] << 13)) ^ (W[i-2] >> 10);
      W[i] = W[i-16] + S0 + W[i-7] + S1;
    }

    a = H[0]; b = H[1]; c = H[2]; d = H[3];
    e = H[4]; f = H[5]; g = H[6]; h = H[7];

    for(i = 0; i < 64; i++){
      S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
      ch = (e & f) ^ (~e & g);
      t1 = h + S1 + ch + K[i] + W[i];
      S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
      maj = (a & b) ^ (a & c) ^ (b & c);
      t2 = S0 + maj;

      h = g; g = f; f = e; e = d + t1;
      d = c; c = b; b = a; a = t1 + t2;
    }

    H[0] += a; H[1] += b; H[2] += c; H[3] += d;
    H[4] += e; H[5] += f; H[6] += g; H[7] += h;
  }

  for(i = 0; i < 8; i++)
    for(int shift = 28; shift >= 0; shift -= 4)
      *out_hex++ = hex[(H[i] >> shift) & 0xf];
  *out_hex = 0;
}

// called from forkret() during first process init. creates /etc directory and
// /etc/passwd if they don't exist, then seeds admin/patient1/doctor1 accounts.
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

// reads /etc/passwd, hashes input pw, compares against stored hash.
// on success populates proc->uid/gid/role/username/authenticated.
// returns 0 on success, -1 on bad credentials.
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

// admin-only (uid==0). appends one credential to /etc/passwd.
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

// admin-only. compacts credential array in-place, skipping the target entry.
// refuses to delete the admin account.
int
auth_userdel(char *username)
{
  struct proc *p = myproc();
  struct credential creds[MAX_USERS];
  int count, out = 0, found = 0;

  if(p == 0 || p->uid != 0 || !p->authenticated || emptystr(username))
    return -1;
  if(streq(username, "admin"))  // admin account is permanent
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

// non-admin users must supply correct old password; admin can bypass old-pw check
// and reset anyone's password.
int
auth_passwd(char *username, char *old_pw, char *new_pw)
{
  struct proc *p = myproc();
  struct credential creds[MAX_USERS];
  char old_hash[HASH_LEN + 1];
  int count;

  if(p == 0 || !p->authenticated || emptystr(username) || emptystr(new_pw))
    return -1;
  if(p->uid != 0 && !streq(username, p->username))  // non-admin can only change own pw
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

// formats "username uid=X gid=Y role=Z\n" into caller-supplied buffer.
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