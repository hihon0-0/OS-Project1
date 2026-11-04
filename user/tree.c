#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

// Print indentation: 2 spaces per depth level
static void
print_indent(int depth)
{
  for(int i = 0; i < depth; i++){
    printf("  ");
  }
}

void
tree(char *path, int depth)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "tree: cannot open %s\n", path);
    if(depth == 0)
      exit(1);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "tree: cannot stat %s\n", path);
    close(fd);
    if(depth == 0)
      exit(1);
    return;
  }

  if(st.type != T_DIR){
    // If root argument is a regular file
    printf("%s\n", path);
    close(fd);
    return;
  }

  // Print root directory name at depth 0
  if(depth == 0){
    int len = strlen(path);
    if(len > 0 && path[len - 1] == '/')
      printf("%s\n", path);
    else
      printf("%s/\n", path);
  }

  if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
    fprintf(2, "tree: path too long\n");
    close(fd);
    return;
  }

  strcpy(buf, path);
  p = buf + strlen(buf);
  if(p > buf && *(p - 1) != '/'){
    *p++ = '/';
  }

  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0)
      continue;

    // Ensure null-terminated entry name
    char name[DIRSIZ + 1];
    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = 0;

    // Skip special directories "." and ".."
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    // Build child path
    if((p - buf) + strlen(name) + 1 > sizeof(buf)){
      fprintf(2, "tree: path too long\n");
      continue;
    }
    strcpy(p, name);

    struct stat st_child;
    if(stat(buf, &st_child) < 0){
      fprintf(2, "tree: cannot stat %s\n", buf);
      continue;
    }

    // Print indentation for child
    print_indent(depth + 1);

    if(st_child.type == T_DIR){
      printf("%s/\n", name);
      // Recursively traverse subdirectory
      tree(buf, depth + 1);
    } else {
      printf("%s\n", name);
    }
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 2){
    tree(".", 0);
  } else {
    tree(argv[1], 0);
  }
  exit(0);
}
