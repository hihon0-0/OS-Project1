#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define LINE_SIZE 1024

// Run once per input line, splitting its words on spaces and tabs.
static int
run_line(char *line, char **args, int base)
{
  int count = base;
  char *p = line;

  while(*p){
    while(*p == ' ' || *p == '\t')
      p++;
    if(*p == '\0')
      break;
    // Reserve one slot for the null pointer required by exec.
    if(count >= MAXARG - 1){
      fprintf(2, "xargs: too many arguments\n");
      return 1;
    }
    args[count++] = p;
    while(*p && *p != ' ' && *p != '\t')
      p++;
    if(*p)
      *p++ = '\0';
  }

  // Empty and whitespace-only lines do not invoke the command.
  if(count == base)
    return 0;
  args[count] = 0;

  int pid = fork();
  if(pid < 0){
    fprintf(2, "xargs: fork failed\n");
    return 1;
  }
  if(pid == 0){
    exec(args[0], args);
    // A successful exec never returns.
    fprintf(2, "xargs: exec %s failed\n", args[0]);
    exit(1);
  }

  int status = 0;
  if(wait(&status) < 0){
    fprintf(2, "xargs: wait failed\n");
    return 1;
  }
  return status != 0;
}

int
main(int argc, char *argv[])
{
  char *args[MAXARG];
  char line[LINE_SIZE];
  char c;
  int first = 1;
  int used = 0;
  int failed = 0;
  int overflow = 0;
  int n;

  // Compatibility option for this lab: still execute once per line.
  // This does not implement general UNIX xargs -n argument batching.
  if(argc > 1 && strcmp(argv[1], "-n") == 0){
    if(argc < 4 || strcmp(argv[2], "1") != 0){
      fprintf(2, "usage: xargs [-n 1] command [args ...]\n");
      exit(1);
    }
    first = 3;
  }
  if(argc <= first){
    fprintf(2, "usage: xargs [-n 1] command [args ...]\n");
    exit(1);
  }

  int base = argc - first;
  // Leave room for at least one input argument and the null pointer.
  if(base > MAXARG - 2){
    fprintf(2, "xargs: too many command arguments\n");
    exit(1);
  }
  for(int i = 0; i < base; i++)
    args[i] = argv[first + i];

  while((n = read(0, &c, 1)) > 0){
    if(c == '\n'){
      if(!overflow){
        line[used] = '\0';
        if(run_line(line, args, base))
          failed = 1;
      }
      used = 0;
      overflow = 0;
    } else if(!overflow){
      if(c == '\0'){
        fprintf(2, "xargs: null byte in input line\n");
        failed = 1;
        overflow = 1;
      } else if(used >= LINE_SIZE - 1){
        fprintf(2, "xargs: input line too long\n");
        failed = 1;
        overflow = 1;
      } else {
        line[used++] = c;
      }
    }
  }

  if(n < 0){
    fprintf(2, "xargs: read failed\n");
    failed = 1;
  } else if(used > 0 && !overflow){
    // EOF can terminate the last line without a newline.
    line[used] = '\0';
    if(run_line(line, args, base))
      failed = 1;
  }
  exit(failed);
}
