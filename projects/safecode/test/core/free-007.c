// RUN: test.sh -e -t %t %s
//
// TEST: free-007
//
// Description:
//  Test invalid memory deallocations
//

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  char * array = calloc (1024, 10);
  array = realloc (array, 10);
  free (&(array[5]));
  return 0;
}

