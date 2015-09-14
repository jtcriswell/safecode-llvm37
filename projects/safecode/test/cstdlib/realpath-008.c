// RUN: test.sh -e -t %t %s

#include <stdlib.h>

//
// Off-by-one buffer overflow caused by realpath().
//

int main() {
  char buf[1];
  realpath ("/", buf);
  return 0;
}
