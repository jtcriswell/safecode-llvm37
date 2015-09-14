// RUN: test.sh -e -t %t %s
#include <string.h>

// Call memchr() on a bytestring with the result of reading out of bounds.

int main()
{
  char b[]  = "dddddddddd";
  char c[7] = "ABCDEF";
  char d[]  = "dddddddddd";
  char *pt;
  pt = memchr(&c[0], 'd', 10);
  return 0;
}
