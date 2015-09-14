// RUN: test.sh -p -t %t %s

#include <assert.h>
#include <errno.h>
#include <unistd.h>

//
// Example of using getcwd() with error conditions.
//

int main() {
  char buf[1], *cwd;

  chdir("/");

  cwd = getcwd(buf, sizeof(buf));

  assert(cwd == NULL);
  assert(errno == ERANGE);

  return 0;
}
