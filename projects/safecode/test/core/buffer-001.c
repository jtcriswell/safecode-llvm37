// RUN: test.sh -e -t %t %s
//
// TEST: buffer-001
//
// Description:
//  Test that an off-by-one read on a global is detected.
//

#include <stdio.h>
#include <stdlib.h>

static char array[1024];

int
main (int argc, char ** argv) {
  int value = array[1024];
  printf("the value is %d\n", value);
  
  return 0;
}

