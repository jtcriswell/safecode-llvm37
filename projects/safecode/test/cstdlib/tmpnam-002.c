// RUN: test.sh -e -t %t %s

#include <stdio.h>

// Buffer overflow with tmpnam().

int main()
{
  char buf[L_tmpnam];

  tmpnam(&buf[1]);

  return 0;
}
