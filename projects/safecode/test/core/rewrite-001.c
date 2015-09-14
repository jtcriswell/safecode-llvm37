// RUN: test.sh -p -t %t %s
//
// TEST: rewrite-001
//
// Description:
//  Test that indexing out of and then back into a locally stack-allocated
//  object does not trigger a memory safety error.  This checks the rewrite
//  pointer functionality for exactcheck().
//

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

volatile unsigned int value = 0;

int
main (int argc, char ** argv) {
  /*
   * Check the number of command line arguments.
   */
  if (argc < 3) {
    execl (argv[0], argv[0], "2", "2", 0);
    return 1;
  }

  /*
   * Get the number of bytes to add and subtract from the command line.
   * This ensures that optimization cannot make this test pass.
   */
  unsigned int index1 = atoi (argv[1]);
  unsigned int index2 = atoi (argv[2]);
  printf ("Indices: %d %d\n", index1, index2);
  fflush (stdout);

  /*
   * Move a pointer that can be checked by exactcheck out of and then back
   * into its referent.
   */
  unsigned char foo = 11;
  unsigned char *bar = (unsigned char*)&foo;

  bar += index1;

  bar -= index2;

  value = *bar;
  printf ("value is %d\n", value);
  return 0;
}

