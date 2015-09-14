// RUN: test.sh -p -t %t %s

#include <assert.h>
#include <stdlib.h>

//
// Use realpath() with a NULL buffer parameter; this should return allocated
// memory.
//

int main() {
  char * p = realpath ("/etc/passwd", NULL);
  assert (p);
  free (p);
  return 0;
}
