// RUN: test.sh -e -t %t %s
//
// TEST: free-002
//
// Description:
//  Test invalid memory deallocations
//

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  volatile char array[1024];
  free (array);
  return 0;
}

