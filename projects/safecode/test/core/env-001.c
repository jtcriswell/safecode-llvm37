// RUN: test.sh -e -t %t %s silly
//
// TEST: env-001
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
  for (index = 0; index < strlen (env[0]) + 5; ++index) {
    printf ("%c %c", env[0][index], argv[0]);
  }

  return 0;
}

