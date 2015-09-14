// RUN: test.sh -e -t %t %s

// bcopy() with a too small destination.

#include <strings.h>

int main()
{
  char buf1[11], buf2[10];
  bcopy(buf1, buf2, 11);
  return 0;
}
