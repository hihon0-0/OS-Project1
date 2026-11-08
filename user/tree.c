#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"

// Maximum directory depth supported. In xv6, MAXPATH is 128,
// so any path cannot exceed ~64 levels (minimum 2 chars per level).
#define MAX_TREE_DEPTH 64

// State tracked per directory level in explicit iterative stack.
struct level_state {
  int base_len;   // Length of pathbuf prefix for this directory (including trailing '/')
  int entry_idx;  // Number of dirent slots read so far in this directory
};

// Global buffers in BSS to guarantee 0 stack consumption across depth levels.
static char pathbuf[MAXPATH + DIRSIZ + 4];
static struct level_state stack[MAX_TREE_DEPTH];
static char skipbuf[512];

// Print indentation: 2 spaces per depth level.
static void
print_indent(int depth)
{
  for(int i = 0; i < depth; i++){
    write(1, "  ", 2);
  }
}

// Iterative tree traversal.
// Completely eliminates both stack overflow (0 recursion, O(1) stack space)
// and file descriptor exhaustion (at most 1 open directory FD at any time).
static void
tree_iterative(char *init_path)
{
  int fd;
  struct stat st;
  struct dirent de;

  if((fd = open(init_path, O_RDONLY)) < 0){
    fprintf(2, "tree: cannot open %s\n", init_path);
    exit(1);
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "tree: cannot stat %s\n", init_path);
    close(fd);
    exit(1);
  }

  // If initial path is a regular file or device, print name and exit.
  if(st.type != T_DIR){
    printf("%s\n", init_path);
    close(fd);
    return;
  }

  // Print root directory name.
  int init_len = strlen(init_path);
  if(init_len > 0 && init_path[init_len - 1] == '/')
    printf("%s\n", init_path);
  else
    printf("%s/\n", init_path);

  close(fd);

  // Initialize shared pathbuf.
  memmove(pathbuf, init_path, init_len);
  int cur_len = init_len;
  if(cur_len > 0 && pathbuf[cur_len - 1] != '/'){
    pathbuf[cur_len++] = '/';
  }
  pathbuf[cur_len] = '\0';

  // Push root directory onto level stack.
  int depth = 0;
  stack[0].base_len = cur_len;
  stack[0].entry_idx = 0;

  // Active directory file descriptor.
  fd = -1;

  while(depth >= 0){
    int base_len = stack[depth].base_len;

    // Restore pathbuf to current directory prefix.
    pathbuf[base_len] = '\0';

    // Open directory if not already open.
    if(fd < 0){
      if((fd = open(pathbuf, O_RDONLY)) < 0){
        fprintf(2, "tree: cannot open %s\n", pathbuf);
        depth--;
        continue;
      }
      // Fast block-buffered advance to where we left off in this directory (reduces syscall overhead).
      int to_skip = stack[depth].entry_idx * sizeof(struct dirent);
      while(to_skip > 0){
        int chunk = to_skip > sizeof(skipbuf) ? sizeof(skipbuf) : to_skip;
        int n = read(fd, skipbuf, chunk);
        if(n <= 0)
          break;
        to_skip -= n;
      }
    }

    // Read next entry.
    int found_entry = 0;
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      stack[depth].entry_idx++;

      if(de.inum == 0)
        continue;

      // Null-terminate entry name safely (DIRSIZ = 14).
      char name[DIRSIZ + 1];
      memmove(name, de.name, DIRSIZ);
      name[DIRSIZ] = '\0';

      // Skip "." and ".."
      if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        continue;

      int namelen = strlen(name);
      if(base_len + namelen + 1 >= MAXPATH){
        fprintf(2, "tree: path too long: %s%s\n", pathbuf, name);
        continue;
      }

      // Append child name to pathbuf.
      memmove(pathbuf + base_len, name, namelen);
      pathbuf[base_len + namelen] = '\0';

      if(stat(pathbuf, &st) < 0){
        fprintf(2, "tree: cannot stat %s\n", pathbuf);
        pathbuf[base_len] = '\0';
        continue;
      }

      print_indent(depth + 1);

      if(st.type == T_DIR){
        printf("%s/\n", name);

        // Verify stack depth limit BEFORE closing current directory fd!
        // This ensures remaining siblings in current directory are NOT skipped.
        if(depth + 1 >= MAX_TREE_DEPTH){
          fprintf(2, "tree: maximum depth exceeded: %s\n", pathbuf);
          pathbuf[base_len] = '\0';
          continue;
        }

        // Close current directory FD before descending.
        close(fd);
        fd = -1;

        depth++;
        int new_len = base_len + namelen;
        pathbuf[new_len++] = '/';
        pathbuf[new_len] = '\0';
        stack[depth].base_len = new_len;
        stack[depth].entry_idx = 0;

        found_entry = 1;
        break; // Descend to new level
      } else {
        printf("%s\n", name);
        pathbuf[base_len] = '\0';
      }
    }

    // If we descended, loop back to process child.
    if(found_entry)
      continue;

    // No more entries at this level: close fd and backtrack to parent.
    if(fd >= 0){
      close(fd);
      fd = -1;
    }
    depth--;
  }
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

  tree_iterative(path);
  exit(0);
}
