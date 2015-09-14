// RUN: test.sh -e -t %t %s
//
// TEST: free-008
// XFAIL: darwin,linux
//
// Description:
//  Test invalid memory deallocations
//

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  free (argv);
  return 0;
}

