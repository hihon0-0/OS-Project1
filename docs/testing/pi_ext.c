#include "kernel/types.h"
#include "kernel/procinfo.h"
#include "user/user.h"

// State values returned by kernel/proc.h.
enum { PI_SLEEPING = 2, PI_RUNNABLE = 3, PI_RUNNING = 4 };
#define PAGE_SIZE 4096

static int failures;

static void
check(int ok, char *label)
{
  if(!ok){
    printf("FAIL: %s\n", label);
    failures++;
    return;
  }
  printf("PASS: %s\n", label);
}

int
main(int argc, char **argv)
{
  struct procinfo info, expected;
  int pid = getpid();
  char *name = argv[0], *brk;
  uint64 original_size;
  int ready[2], release[2], child, status;
  char token;

  (void)argc;
  for(char *s = argv[0]; *s; s++)
    if(*s == '/')
      name = s + 1;

  failures = 0;
  printf("=== PROCINFO TEST ===\n");
  memset(&info, 0xa5, sizeof(info));
  check(procinfo(pid, &info) == 0, "query current process");
  check(info.pid == pid, "current PID");
  check(info.ppid > 0, "current PPID");
  check(info.state == PI_RUNNING, "current state is RUNNING");
  check(strcmp(info.name, name) == 0, "current process name");
  brk = sbrk(0);
  check(brk != (char *)-1 && info.sz == (uint64)brk,
        "size equals current program break");
  original_size = info.sz;

  // Check ABI padding and the unused bytes after name's NUL.
  memset(&expected, 0, sizeof(expected));
  expected.pid = info.pid;
  expected.ppid = info.ppid;
  expected.state = info.state;
  expected.sz = info.sz;
  strcpy(expected.name, name);
  check(memcmp(&info, &expected, sizeof(info)) == 0,
        "padding and unused name bytes are zero");

  check(sbrk(PAGE_SIZE) == brk, "grow address space");
  check(procinfo(pid, &info) == 0 && info.sz == original_size + PAGE_SIZE,
        "size after sbrk growth");
  check(sbrk(-PAGE_SIZE) != (char *)-1, "shrink address space");
  check(procinfo(pid, &info) == 0 && info.sz == original_size,
        "size after sbrk shrink");

  check(procinfo(-1, &info) == -1, "negative PID rejected");
  check(procinfo(0, &info) == -1, "zero PID rejected");
  check(procinfo(99999, &info) == -1, "nonexistent PID rejected");

  check(pipe(ready) == 0 && pipe(release) == 0, "create synchronization pipes");
  child = fork();
  if(child < 0)
    check(0, "fork succeeds");
  if(child == 0){
    close(ready[0]);
    close(release[1]);
    if(procinfo(getpid(), &info) != 0 || info.pid != getpid() ||
       info.ppid != pid)
      exit(1);
    if(write(ready[1], "R", 1) != 1 || read(release[0], &token, 1) != 1)
      exit(1);
    close(ready[1]);
    close(release[0]);
    exit(0);
  }
  close(ready[1]);
  close(release[0]);
  check(read(ready[0], &token, 1) == 1,
        "child verifies its PID and PPID");
  check(procinfo(child, &info) == 0 && info.pid == child && info.ppid == pid,
        "parent queries living child");
  // The child can still be entering read() on another CPU.
  check(info.state == PI_SLEEPING || info.state == PI_RUNNABLE ||
        info.state == PI_RUNNING, "living child state");
  check(info.sz == original_size && strcmp(info.name, name) == 0,
        "child inherits size and name");
  check(write(release[1], "X", 1) == 1, "release child");
  close(ready[0]);
  close(release[1]);
  check(wait(&status) == child && status == 0, "reap successful child");
  check(procinfo(child, &info) == -1, "reaped PID no longer exists");

  check(procinfo(pid, (struct procinfo *)0) == -1, "null pointer rejected");
  check(procinfo(pid, (struct procinfo *)~(uint64)0) == -1,
        "address outside user space rejected");

  // End at a page boundary, followed by an unmapped page.
  brk = sbrk(0);
  int extra = PAGE_SIZE - ((uint64)brk % PAGE_SIZE);
  check(sbrk(extra) == brk, "prepare page boundary");
  uint64 end = (uint64)brk + extra;
  check(procinfo(pid, (struct procinfo *)end) == -1,
        "unmapped page rejected");
  check(procinfo(pid, (struct procinfo *)(end - sizeof(info) / 2)) == -1,
        "buffer crossing into unmapped page rejected");
  // copyout may modify the valid prefix before rejecting the suffix.
  check(sbrk(-extra) != (char *)-1, "restore program break");
  check(procinfo(pid, &info) == 0 && info.sz == original_size,
        "valid call still works after errors");

  printf("=== PROCINFO EXTENDED FAILURES: %d ===\n", failures);
  exit(0);
}
