// RUN: test.sh -p -t %t %s silly
//
// TEST: env-002
//
// Description:
//  Test that array bounds checking works on environment strings
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (int argc, char ** argv, char ** env) {
  int index = 0;
  for (index = 0; index < strlen (env[0]); ++index) {
    printf ("%c", env[0][index]);
  }

  return 0;
}

