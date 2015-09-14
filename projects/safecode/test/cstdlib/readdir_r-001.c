// RUN: test.sh -p -t %t %s

#include <dirent.h>
#include <assert.h>

// Example of correct usage of readdir_r()

int main()
{
  DIR *root = opendir("/");
  struct dirent entry, *result;
  assert(readdir_r(root, &entry, &result) == 0);
  closedir(root);
  return 0;
}
