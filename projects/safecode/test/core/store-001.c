// RUN: test.sh -e -t %t %s
//
// TEST: store-001
//
// Description:
//  Test that a memory access that falls off the end of a memory object causes
//  a memory safety error to be reported.
//

#include <stdio.h>
#include <stdlib.h>

char value;

int
main (int argc, char ** argv) {
  int * p = (int *)(&value);
  *p = argc;
  printf ("the value is %d\n", *p);
  
  return 0;
}

