// RUN: test.sh -e -t %t %s

#include <unistd.h>
#include <stdlib.h>

//
// getcwd() causing a potential buffer overflow by being given a buffer size
// that is too large.
//

int main() {
  char *buf = malloc(10);
  getcwd(&buf[2], 10000);
  return 0;
}
