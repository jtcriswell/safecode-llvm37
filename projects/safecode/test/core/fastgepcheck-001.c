// RUN: test.sh -p -t %t %s

/* exactcheck() move pointer in and out of bounds test */

#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char ** argv) {
  unsigned char x = 0;
  unsigned char *y = (unsigned char*)&x + 2 - 2;
  printf ("%p %p\n", y, &x);
  fflush (stdout);
  return *y;
}

