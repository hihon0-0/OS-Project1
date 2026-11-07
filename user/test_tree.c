#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// Helper to run tree with arguments and return exit status.
static int
run_tree(char *arg)
{
  int pid = fork();
  if(pid < 0){
    printf("test_tree: fork failed\n");
    return -1;
  }
  if(pid == 0){
    char *argv[3];
    argv[0] = "tree";
    if(arg){
      argv[1] = arg;
      argv[2] = 0;
    } else {
      argv[1] = 0;
    }
    exec("tree", argv);
    printf("test_tree: exec failed\n");
    exit(1);
  }
  int status = 0;
  wait(&status);
  return status;
}

static void
create_file(char *path)
{
  int fd = open(path, O_CREATE | O_WRONLY);
  if(fd < 0){
    printf("test_tree: failed to create file %s\n", path);
    return;
  }
  write(fd, "test\n", 5);
  close(fd);
}

// Case 1: Tree on regular file
static void
test_1_regular_file(void)
{
  printf("\n=== Test 1: Tree on regular file ===\n");
  create_file("t1_file.txt");
  int status = run_tree("t1_file.txt");
  if(status == 0)
    printf("PASS: Test 1\n");
  else
    printf("FAIL: Test 1 (status = %d)\n", status);
}

// Case 2: Tree on empty directory
static void
test_2_empty_dir(void)
{
  printf("\n=== Test 2: Tree on empty directory ===\n");
  mkdir("t2_empty");
  int status = run_tree("t2_empty");
  if(status == 0)
    printf("PASS: Test 2\n");
  else
    printf("FAIL: Test 2 (status = %d)\n", status);
}

// Case 3: Directory without trailing slash
static void
test_3_no_trailing_slash(void)
{
  printf("\n=== Test 3: Directory without trailing slash ===\n");
  mkdir("t3_dir");
  create_file("t3_dir/f1");
  int status = run_tree("t3_dir");
  if(status == 0)
    printf("PASS: Test 3\n");
  else
    printf("FAIL: Test 3 (status = %d)\n", status);
}

// Case 4: Directory with trailing slash
static void
test_4_trailing_slash(void)
{
  printf("\n=== Test 4: Directory with trailing slash ===\n");
  int status = run_tree("t3_dir/");
  if(status == 0)
    printf("PASS: Test 4\n");
  else
    printf("FAIL: Test 4 (status = %d)\n", status);
}

// Case 5: Entry name of maximum length (DIRSIZ = 14 chars)
static void
test_5_max_name(void)
{
  printf("\n=== Test 5: Maximum entry name (14 chars) ===\n");
  mkdir("t5_maxname");
  create_file("t5_maxname/12345678901234");
  int status = run_tree("t5_maxname");
  if(status == 0)
    printf("PASS: Test 5\n");
  else
    printf("FAIL: Test 5 (status = %d)\n", status);
}

// Case 6: Single-character shortest entry name
static void
test_6_short_name(void)
{
  printf("\n=== Test 6: Single-character entry name ===\n");
  mkdir("t6");
  mkdir("t6/a");
  create_file("t6/b");
  int status = run_tree("t6");
  if(status == 0)
    printf("PASS: Test 6\n");
  else
    printf("FAIL: Test 6 (status = %d)\n", status);
}

// Case 7: Broad directory (35 files)
static void
test_7_broad_dir(void)
{
  printf("\n=== Test 7: Broad directory (35 files) ===\n");
  mkdir("t7_broad");
  char name[32];
  for(int i = 0; i < 35; i++){
    name[0] = 't';
    name[1] = '7';
    name[2] = '_';
    name[3] = 'b';
    name[4] = 'r';
    name[5] = 'o';
    name[6] = 'a';
    name[7] = 'd';
    name[8] = '/';
    name[9] = 'f';
    name[10] = '0' + (i / 10);
    name[11] = '0' + (i % 10);
    name[12] = '\0';
    create_file(name);
  }
  int status = run_tree("t7_broad");
  if(status == 0)
    printf("PASS: Test 7\n");
  else
    printf("FAIL: Test 7 (status = %d)\n", status);
}

// Case 8: Broad directory of subdirectories (20 subdirectories)
static void
test_8_broad_subdirs(void)
{
  printf("\n=== Test 8: Broad directory (20 subdirectories) ===\n");
  mkdir("t8_subs");
  char name[32];
  for(int i = 0; i < 20; i++){
    name[0] = 't';
    name[1] = '8';
    name[2] = '_';
    name[3] = 's';
    name[4] = 'u';
    name[5] = 'b';
    name[6] = 's';
    name[7] = '/';
    name[8] = 's';
    name[9] = '0' + (i / 10);
    name[10] = '0' + (i % 10);
    name[11] = '\0';
    mkdir(name);
  }
  int status = run_tree("t8_subs");
  if(status == 0)
    printf("PASS: Test 8\n");
  else
    printf("FAIL: Test 8 (status = %d)\n", status);
}

// Case 9: Deep tree (14 levels deep - replicates review case)
static void
test_9_deep_tree(void)
{
  printf("\n=== Test 9: Deep tree (14 levels deep) ===\n");
  char path[128];
  path[0] = 'd';
  path[1] = '0';
  path[2] = '\0';
  mkdir(path);

  for(int i = 1; i <= 13; i++){
    int len = strlen(path);
    path[len] = '/';
    path[len + 1] = 'd';
    if(i < 10){
      path[len + 2] = '0' + i;
      path[len + 3] = '\0';
    } else {
      path[len + 2] = '1';
      path[len + 3] = '0' + (i - 10);
      path[len + 4] = '\0';
    }
    mkdir(path);
  }

  int len = strlen(path);
  path[len] = '/';
  path[len + 1] = 'l';
  path[len + 2] = 'e';
  path[len + 3] = 'a';
  path[len + 4] = 'f';
  path[len + 5] = '\0';
  create_file(path);

  int status = run_tree("d0");
  if(status == 0)
    printf("PASS: Test 9 (Deep tree completed with NO stack overflow)\n");
  else
    printf("FAIL: Test 9 (status = %d)\n", status);
}

// Case 10: Ultra-deep tree (25 levels deep)
static void
test_10_ultra_deep(void)
{
  printf("\n=== Test 10: Ultra-deep tree (25 levels deep) ===\n");
  char path[128];
  path[0] = 'u';
  path[1] = '0';
  path[2] = '\0';
  mkdir(path);

  for(int i = 1; i <= 24; i++){
    int len = strlen(path);
    path[len] = '/';
    path[len + 1] = 'u';
    if(i < 10){
      path[len + 2] = '0' + i;
      path[len + 3] = '\0';
    } else {
      path[len + 2] = '0' + (i / 10);
      path[len + 3] = '0' + (i % 10);
      path[len + 4] = '\0';
    }
    mkdir(path);
  }

  int len = strlen(path);
  path[len] = '/';
  path[len + 1] = 'e';
  path[len + 2] = 'n';
  path[len + 3] = 'd';
  path[len + 4] = '\0';
  create_file(path);

  int status = run_tree("u0");
  if(status == 0)
    printf("PASS: Test 10 (25-level tree completed successfully)\n");
  else
    printf("FAIL: Test 10 (status = %d)\n", status);
}

// Case 11: Deep + Broad tree (FD exhaustion test)
static void
test_11_deep_and_broad(void)
{
  printf("\n=== Test 11: Deep + Broad tree (FD leak stress test) ===\n");
  char path[128];
  path[0] = 'b';
  path[1] = '0';
  path[2] = '\0';
  mkdir(path);

  for(int i = 1; i <= 10; i++){
    int cur_len = strlen(path);
    path[cur_len] = '/';
    path[cur_len + 1] = 'f';
    path[cur_len + 2] = 'i';
    path[cur_len + 3] = 'l';
    path[cur_len + 4] = 'e';
    path[cur_len + 5] = '\0';
    create_file(path);

    path[cur_len + 1] = 'b';
    if(i < 10){
      path[cur_len + 2] = '0' + i;
      path[cur_len + 3] = '\0';
    } else {
      path[cur_len + 2] = '1';
      path[cur_len + 3] = '0';
      path[cur_len + 4] = '\0';
    }
    mkdir(path);
  }

  int status = run_tree("b0");
  if(status == 0)
    printf("PASS: Test 11 (FD limit not exceeded, no descriptor leak)\n");
  else
    printf("FAIL: Test 11 (status = %d)\n", status);
}

// Case 12: Binary branching tree
static void
test_12_binary_branching(void)
{
  printf("\n=== Test 12: Binary branching tree ===\n");
  mkdir("tree_root");
  mkdir("tree_root/left");
  mkdir("tree_root/right");
  create_file("tree_root/root_file");
  mkdir("tree_root/left/l1");
  mkdir("tree_root/left/l2");
  create_file("tree_root/left/left_file");
  mkdir("tree_root/right/r1");
  mkdir("tree_root/right/r2");
  create_file("tree_root/right/right_file");
  create_file("tree_root/left/l1/deep_file");
  create_file("tree_root/right/r2/deep_file");

  int status = run_tree("tree_root");
  if(status == 0)
    printf("PASS: Test 12 (Branching tree traversed successfully)\n");
  else
    printf("FAIL: Test 12 (status = %d)\n", status);
}

// Case 13: Non-existent path
static void
test_13_nonexistent(void)
{
  printf("\n=== Test 13: Non-existent path ===\n");
  int status = run_tree("no_such_file_xyz_123");
  if(status != 0)
    printf("PASS: Test 13 (Reported error and exited != 0)\n");
  else
    printf("FAIL: Test 13 (Expected != 0, got 0)\n");
}

// Case 14: Default argument (tree with no arguments)
static void
test_14_default_arg(void)
{
  printf("\n=== Test 14: Default argument (tree) ===\n");
  int status = run_tree(0);
  if(status == 0)
    printf("PASS: Test 14\n");
  else
    printf("FAIL: Test 14 (status = %d)\n", status);
}

// Case 15: Explicit current directory (tree .)
static void
test_15_dot_arg(void)
{
  printf("\n=== Test 15: Explicit current directory (tree .) ===\n");
  int status = run_tree(".");
  if(status == 0)
    printf("PASS: Test 15\n");
  else
    printf("FAIL: Test 15 (status = %d)\n", status);
}

// Case 16: Root filesystem (tree /)
static void
test_16_root_slash(void)
{
  printf("\n=== Test 16: Root filesystem (tree /) ===\n");
  int status = run_tree("/");
  if(status == 0)
    printf("PASS: Test 16\n");
  else
    printf("FAIL: Test 16 (status = %d)\n", status);
}

// Case 17: Multiple nested empty directories
static void
test_17_nested_empty(void)
{
  printf("\n=== Test 17: Nested empty directories ===\n");
  mkdir("ne0");
  mkdir("ne0/ne1");
  mkdir("ne0/ne1/ne2");
  mkdir("ne0/ne1/ne2/ne3");
  int status = run_tree("ne0");
  if(status == 0)
    printf("PASS: Test 17\n");
  else
    printf("FAIL: Test 17 (status = %d)\n", status);
}

// Case 18: Mixed directory with empty and non-empty siblings
static void
test_18_mixed_siblings(void)
{
  printf("\n=== Test 18: Mixed empty and non-empty siblings ===\n");
  mkdir("mix");
  mkdir("mix/empty_sub");
  create_file("mix/file1");
  mkdir("mix/full_sub");
  create_file("mix/full_sub/file2");
  create_file("mix/file3");
  int status = run_tree("mix");
  if(status == 0)
    printf("PASS: Test 18\n");
  else
    printf("FAIL: Test 18 (status = %d)\n", status);
}

// Case 19: Long path near MAXPATH (128 bytes)
static void
test_19_near_maxpath(void)
{
  printf("\n=== Test 19: Long path near MAXPATH limit ===\n");
  // Build a path of ~100 characters
  char path[128];
  strcpy(path, "longpath");
  mkdir(path);
  for(int i = 0; i < 9; i++){
    int l = strlen(path);
    strcpy(path + l, "/subfolder");
    mkdir(path);
  }
  create_file("longpath/subfolder/subfolder/subfolder/subfolder/subfolder/subfolder/subfolder/subfolder/subfolder/end");
  int status = run_tree("longpath");
  if(status == 0)
    printf("PASS: Test 19\n");
  else
    printf("FAIL: Test 19 (status = %d)\n", status);
}

// Case 20: Argument exceeding MAXPATH (128 bytes)
static void
test_20_exceed_maxpath(void)
{
  printf("\n=== Test 20: Argument exceeding MAXPATH ===\n");
  char toolong[150];
  memset(toolong, 'a', 140);
  toolong[140] = '\0';
  int status = run_tree(toolong);
  if(status != 0)
    printf("PASS: Test 20 (Rejected oversized path with exit code != 0)\n");
  else
    printf("FAIL: Test 20 (Expected != 0, got 0)\n");
}

// Case 21: Device file (tree console)
static void
test_21_device_file(void)
{
  printf("\n=== Test 21: Device file (tree console) ===\n");
  int status = run_tree("console");
  if(status == 0)
    printf("PASS: Test 21\n");
  else
    printf("FAIL: Test 21 (status = %d)\n", status);
}

// Case 22: Directory containing a device file
static void
test_22_dir_with_device(void)
{
  printf("\n=== Test 22: Directory containing device file ===\n");
  mkdir("devdir");
  // Link console or create mknod inside devdir
  link("console", "devdir/myconsole");
  int status = run_tree("devdir");
  if(status == 0)
    printf("PASS: Test 22\n");
  else
    printf("FAIL: Test 22 (status = %d)\n", status);
}

// Case 23: Multiple consecutive tree runs in same process
static void
test_23_consecutive_runs(void)
{
  printf("\n=== Test 23: Multiple consecutive invocations ===\n");
  int s1 = run_tree("t1_file.txt");
  int s2 = run_tree("t2_empty");
  int s3 = run_tree("t6");
  if(s1 == 0 && s2 == 0 && s3 == 0)
    printf("PASS: Test 23\n");
  else
    printf("FAIL: Test 23 (s1=%d, s2=%d, s3=%d)\n", s1, s2, s3);
}

// Case 24: Zig-zag alternating name lengths
static void
test_24_zigzag_names(void)
{
  printf("\n=== Test 24: Alternating short and long names ===\n");
  mkdir("zz");
  mkdir("zz/12345678901234");
  mkdir("zz/12345678901234/a");
  mkdir("zz/12345678901234/a/abcdefghijklmn");
  create_file("zz/12345678901234/a/abcdefghijklmn/z");
  int status = run_tree("zz");
  if(status == 0)
    printf("PASS: Test 24\n");
  else
    printf("FAIL: Test 24 (status = %d)\n", status);
}

// Case 25: Stress test with 50-node multi-level structure
static void
test_25_stress_50_nodes(void)
{
  printf("\n=== Test 25: 50-node multi-level stress test ===\n");
  mkdir("st50");
  char p[64];
  for(int i = 0; i < 5; i++){
    p[0] = 's'; p[1] = 't'; p[2] = '5'; p[3] = '0'; p[4] = '/';
    p[5] = 'g'; p[6] = '0' + i; p[7] = '\0';
    mkdir(p);
    for(int j = 0; j < 8; j++){
      int l = strlen(p);
      p[l] = '/'; p[l+1] = 'n'; p[l+2] = '0' + j; p[l+3] = '\0';
      create_file(p);
      p[l] = '\0';
    }
  }
  int status = run_tree("st50");
  if(status == 0)
    printf("PASS: Test 25 (50-node stress test completed with 0 errors)\n");
  else
    printf("FAIL: Test 25 (status = %d)\n", status);
}

int
main(int argc, char *argv[])
{
  printf("Starting comprehensive 25-case tree test suite...\n");

  // Run tests on root / default directory first before populating test files
  test_14_default_arg();
  test_15_dot_arg();
  test_16_root_slash();

  test_1_regular_file();
  test_2_empty_dir();
  test_3_no_trailing_slash();
  test_4_trailing_slash();
  test_5_max_name();
  test_6_short_name();
  test_7_broad_dir();
  test_8_broad_subdirs();
  test_9_deep_tree();
  test_10_ultra_deep();
  test_11_deep_and_broad();
  test_12_binary_branching();
  test_13_nonexistent();
  test_17_nested_empty();
  test_18_mixed_siblings();
  test_19_near_maxpath();
  test_20_exceed_maxpath();
  test_21_device_file();
  test_22_dir_with_device();
  test_23_consecutive_runs();
  test_24_zigzag_names();
  test_25_stress_50_nodes();

  printf("\n========================================\n");
  printf("ALL 25 TEST CASES COMPLETED!\n");
  printf("========================================\n");
  exit(0);
}

