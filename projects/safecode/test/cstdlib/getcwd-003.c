// RUN: test.sh -p -t %t %s

#include <assert.h>
#include <unistd.h>
#include <string.h>

//
// Use getcwd() to get a path successfully.
//

int main() {
  char buf[2], *cwd;

  chdir("/");

  cwd = getcwd(buf, sizeof(buf));

  assert(cwd != NULL);
  assert(strcmp(buf, "/") == 0);

  return 0;
}
