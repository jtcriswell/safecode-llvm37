// RUN: test.sh -p -t %t %s
//
// TEST: buffer-002
//
// Description:
//  Test that staying within the bounds of an array never causes a problem.
//

#include <stdio.h>
#include <stdlib.h>

volatile char array[1024];

int
main (int argc, char ** argv) {
  int index;
  int sum;
  for (index = 0; index < 1024; ++index)
    sum += array[index];
  printf ("%d\n", sum);
  return 0;
}

