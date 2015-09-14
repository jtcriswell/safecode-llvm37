// RUN: test.sh -e -t %t %s
//
// TEST: free-004
//
// Description:
//  Test invalid memory deallocations
//

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  char array[1024];
  free (&(array[5]));
  return 0;
}

