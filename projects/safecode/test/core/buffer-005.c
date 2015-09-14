// RUN: test.sh -e -t %t %s
//
// TEST: buffer-005
//
// Description:
//  Test that an off-by-one read on a heap object is detected.
//

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  char * array = malloc (sizeof (char) * 1024);
  return array[1024];
}

