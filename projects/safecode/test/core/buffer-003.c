// RUN: test.sh -e -t %t %s
//
// TEST: buffer-003
//
// Description:
//  Test that an off-by-one read on a global is detected.
//

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  char array[1024];
  return array[1024];
}

