// RUN: test.sh -e -t %t %s silly
//
// TEST: env-003
//
// Description:
//  This is identical to env-001 but doesn't use argv
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main (int argc, char ** argv, char ** env) {
  int index = 0;
  for (index = 0; index < strlen (env[0]) + 5; ++index) {
    printf ("%c", env[0][index]);
  }

  return 0;
}

