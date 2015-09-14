// RUN: test.sh -e -t %t %s

// bcopy() with a too small source.

#include <strings.h>

int main()
{
  char buf1[10], buf2[11];
  bcopy(buf1, buf2, 11);
  return 0;
}
