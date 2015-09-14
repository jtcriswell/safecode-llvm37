// RUN: test.sh -p -t %t %s
//
// TEST: buffer-006
//
// Description:
//  Test that staying within the bounds of an array never causes a problem.
//

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  int index;
  int sum;
  volatile char * array = malloc (sizeof (char) * 1024);
  for (index = 0; index < 1024; ++index)
    sum += array[index];
  printf ("%d\n", sum);
  return 0;
}

