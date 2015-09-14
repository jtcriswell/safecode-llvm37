// RUN: test.sh -e -t %t %s
//
// TEST: buffer-007
//
// Description:
//  Test that an off-by-one read on a global is detected.
//

#include <stdio.h>
#include <stdlib.h>

char array[16];
char *y = array;

int
main (int argc, char ** argv) {
  char *z=y+16;
  printf("the value is %d\n", *z);
  return 0;
}

