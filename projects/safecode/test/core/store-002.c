// RUN: test.sh -e -t %t %s
//
// TEST: store-002
//
// Description:
//  Test that a memory access that falls off the end of a memory object causes
//  a memory safety error to be reported.
//

#include <stdio.h>
#include <stdlib.h>

char value;
volatile int * p;

int
foo (int index) {
  *p = index;
  return *p;
}

int
main (int argc, char ** argv) {
  p = (int *)(&value);
  foo (argc);
  printf ("the value is %d\n", *p);
  
  return 0;
}

