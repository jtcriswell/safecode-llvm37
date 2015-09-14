// RUN: test.sh -e -t %t %s

#include <dirent.h>

// Buffer overflow with readdir_r()

int main()
{
  DIR *root = opendir("/");
  int x;
  struct dirent *result;
  readdir_r(root, (struct dirent *) &x, &result);
  closedir(root);
  return 0;
}
