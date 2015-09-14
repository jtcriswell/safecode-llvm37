// RUN: test.sh -p -t %t %s

#include <unistd.h>

//
// getcwd() with a NULL buffer parameter; this is supported behavior by many
// implementations and returns allocated memory.
//

int main() {
  char *cwd = getcwd(NULL, 100);
  return 0;
}
