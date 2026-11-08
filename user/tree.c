#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// Print indentation: 2 spaces per depth level (Hint 3)
static void
print_indent(int depth)
{
  for(int i = 0; i < depth; i++){
    printf("  ");
  }
}

// Recursively traverse directory tree (Hint 2)
void
tree(char *path, int depth)
{
  char buf[MAXPATH], *p;
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

  // If root argument is a regular file or device (Hint 4)
  if(st.type != T_DIR){
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

  // Check path length to prevent buffer overflow (Hint 6)
  if(strlen(path) + 1 + DIRSIZ + 1 >= sizeof(buf)){
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

    // Ensure null-terminated entry name safely (DIRSIZ = 14)
    char name[DIRSIZ + 1];
    memmove(name, de.name, DIRSIZ);
    name[DIRSIZ] = '\0';

    // Skip special directories "." and ".." to avoid infinite recursion (Hint 5)
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
      continue;

    // Build child path: format parent/child (Hint 6)
    if((p - buf) + strlen(name) + 1 >= sizeof(buf)){
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
      // Recursively traverse subdirectory (Hint 2)
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
  char *path;

  if(argc < 2){
    path = ".";
  } else {
    path = argv[1];
  }

  if(strlen(path) >= MAXPATH){
    fprintf(2, "tree: path too long\n");
    exit(1);
  }

  tree(path, 0);
  exit(0);
}
