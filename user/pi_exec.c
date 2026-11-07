#include "kernel/types.h"
#include "user/user.h"

#ifdef PI_LARGE
static volatile char image[5 * 4096];
#define NEXT_IMAGE "pi_exec_a"
#else
static volatile char image[4096];
#define NEXT_IMAGE "pi_exec_b"
#endif

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

int
main(int argc, char **argv)
{
  image[sizeof(image) - 1] = 1;
  if(argc == 3 && strcmp(argv[1], "size") == 0){
    uint64 size = (uint64)sbrk(0);
    exit(write(atoi(argv[2]), &size, sizeof(size)) != sizeof(size));
  }
  if(argc != 5)
    exit(1);

  int remaining = atoi(argv[1]);
  int ready = atoi(argv[2]);
  int gate = atoi(argv[3]);
  if(strcmp(argv[4], "start") == 0){
    char token;
    if(write(ready, "R", 1) != 1 || read(gate, &token, 1) != 1)
      exit(1);
  }
  if(remaining == 0)
    exit(0);
  if(remaining % 32 == 0)
    sleep(1);

  char count[12];
  number(remaining - 1, count);
  char *next[] = {NEXT_IMAGE, count, argv[2], argv[3], "next", 0};
  exec(next[0], next);
  fprintf(2, "procstress: exec %s failed\n", next[0]);
  exit(1);
}
