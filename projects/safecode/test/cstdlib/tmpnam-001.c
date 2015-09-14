// RUN: test.sh -p -t %t %s

#include <stdio.h>

// Example of the correct usage of tmpnam().

int main()
{
  char buf[L_tmpnam];

  tmpnam(buf);

  tmpnam(NULL);

  return 0;
}
