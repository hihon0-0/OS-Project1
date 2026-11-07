#include "kernel/types.h"
#include "kernel/procinfo.h"
#include "user/user.h"

static void
fail(char *message, int child)
{
  if(child > 0){
    kill(child);
    wait(0);
  }
  printf("FAIL: procstress %s\n", message);
  exit(1);
}

static void
number(int value, char *text)
{
  char digits[12];
  int count = 0;
  do {
    digits[count++] = '0' + value % 10;
    value /= 10;
  } while(value);
  for(int i = 0; i < count; i++)
    text[i] = digits[count - i - 1];
  text[count] = 0;
}

static uint64
image_size(char *path)
{
  int fd[2], status;
  uint64 size;
  if(pipe(fd) < 0)
    fail("calibration pipe", -1);
  int child = fork();
  if(child < 0)
    fail("calibration fork", -1);
  if(child == 0){
    char output[12];
    close(fd[0]);
    number(fd[1], output);
    char *args[] = {path, "size", output, 0};
    exec(path, args);
    exit(1);
  }
  close(fd[1]);
  if(read(fd[0], &size, sizeof(size)) != sizeof(size))
    fail("image size", child);
  close(fd[0]);
  if(wait(&status) != child || status != 0)
    fail("calibration exit", -1);
  return size;
}

static void
exec_test(void)
{
  uint64 a = image_size("pi_exec_a");
  uint64 b = image_size("pi_exec_b");
  if(a == b)
    fail("images must have different sizes", -1);

  int ready[2], gate[2], status;
  if(pipe(ready) < 0 || pipe(gate) < 0)
    fail("exec pipes", -1);
  int child = fork();
  if(child < 0)
    fail("exec fork", -1);
  if(child == 0){
    char output[12], input[12];
    close(ready[0]);
    close(gate[1]);
    number(ready[1], output);
    number(gate[0], input);
    char *args[] = {"pi_exec_a", "400", output, input, "start", 0};
    exec(args[0], args);
    exit(1);
  }
  close(ready[1]);
  close(gate[0]);
  char token;
  if(read(ready[0], &token, 1) != 1)
    fail("exec readiness", child);
  if(write(gate[1], "G", 1) != 1)
    fail("exec start", child);

  int seen = 0, samples = 0;
  int start = uptime();
  while(uptime() - start < 100){
    struct procinfo info;
    if(procinfo(child, &info) != 0 || info.pid != child || info.ppid != getpid())
      fail("exec identity", child);
    if(strcmp(info.name, "pi_exec_a") == 0 && info.sz == a)
      seen |= 1;
    else if(strcmp(info.name, "pi_exec_b") == 0 && info.sz == b)
      seen |= 2;
    else {
      printf("procstress: name=%s size=%ld expected=%ld/%ld\n", info.name, info.sz, a, b);
      fail("mixed exec name and size", child);
    }
    samples++;
  }
  close(ready[0]);
  close(gate[1]);
  if(wait(&status) != child || status != 0)
    fail("exec child status", -1);
  if(seen != 3)
    fail("both exec images must be observed", -1);
  printf("PASS: exec snapshots, %d samples, 400 transitions\n", samples);
}

static void
heap_test(void)
{
  uint64 base = (uint64)sbrk(0);
  int ready[2], gate[2], status;
  if(pipe(ready) < 0 || pipe(gate) < 0)
    fail("heap pipes", -1);
  int child = fork();
  if(child < 0)
    fail("heap fork", -1);
  if(child == 0){
    char token;
    close(ready[0]);
    close(gate[1]);
    if(write(ready[1], "R", 1) != 1 || read(gate[0], &token, 1) != 1)
      exit(1);
    for(int i = 0; i < 1000; i++){
      if(sbrk(4096) == (char *)-1)
        exit(1);
      if(i % 32 == 0)
        sleep(1);
      if(sbrk(-4096) == (char *)-1)
        exit(1);
    }
    exit(0);
  }
  close(ready[1]);
  close(gate[0]);
  char token;
  if(read(ready[0], &token, 1) != 1 || write(gate[1], "G", 1) != 1)
    fail("heap start", child);

  int seen = 0, samples = 0;
  int start = uptime();
  while(uptime() - start < 100){
    struct procinfo info;
    if(procinfo(child, &info) != 0 || info.pid != child || info.ppid != getpid())
      fail("heap identity", child);
    if(info.sz == base)
      seen |= 1;
    else if(info.sz == base + 4096)
      seen |= 2;
    else
      fail("unexpected heap size", child);
    samples++;
  }
  close(ready[0]);
  close(gate[1]);
  if(wait(&status) != child || status != 0)
    fail("heap child status", -1);
  if(seen != 3)
    fail("both heap sizes must be observed", -1);
  printf("PASS: heap snapshots, %d samples, 1000 grow/shrink pairs\n", samples);
}

int
main(void)
{
  exec_test();
  heap_test();
  printf("=== PROCINFO CONCURRENCY TESTS PASSED ===\n");
  exit(0);
}
