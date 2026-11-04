#include "kernel/types.h"
#include "kernel/procinfo.h"
#include "user/user.h"

int
main(void)
{
  struct procinfo info;
  int pid = getpid();
  int child;
  int status = -1;

  printf("=== PROCINFO TEST ===\n");

  // Test 1: Current process
  if(procinfo(pid, &info) < 0){
    printf("FAIL: current process\n");
    exit(1);
  }

  printf("PID: %d\n", info.pid);
  printf("PPID: %d\n", info.ppid);
  printf("State: %d\n", info.state);
  printf("Size: %ld bytes\n", info.sz);
  printf("Name: %s\n", info.name);

  if(info.pid != pid){
    printf("FAIL: incorrect PID\n");
    exit(1);
  }
  printf("PASS: current process\n");

  // Test 2: Negative PID
  if(procinfo(-1, &info) != -1){
    printf("FAIL: negative PID\n");
    exit(1);
  }
  printf("PASS: negative PID rejected\n");

  // Test 3: Nonexistent PID
  if(procinfo(99999, &info) != -1){
    printf("FAIL: nonexistent PID\n");
    exit(1);
  }
  printf("PASS: nonexistent PID rejected\n");

  // Test 4: Parent-child relationship
  child = fork();

  if(child < 0){
    printf("FAIL: fork\n");
    exit(1);
  }

  if(child == 0){
    struct procinfo childinfo;

    if(procinfo(getpid(), &childinfo) == 0 &&
       childinfo.ppid == pid){
      printf("PASS: parent-child relationship\n");
      exit(0);
    }

    printf("FAIL: parent-child relationship\n");
    exit(1);
  }

  if(wait(&status) != child || status != 0){
    printf("FAIL: child test\n");
    exit(1);
  }

  // Test 5: Invalid user pointer
  if(procinfo(getpid(), (struct procinfo *)0) != -1){
    printf("FAIL: null pointer accepted\n");
    exit(1);
  }
  printf("PASS: null pointer rejected\n");

  printf("=== ALL PROCINFO TESTS PASSED ===\n");
  exit(0);
}
