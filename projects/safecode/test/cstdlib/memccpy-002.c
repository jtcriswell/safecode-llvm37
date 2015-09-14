// RUN: test.sh -e -t %t %s
#include <string.h>

// memccpy() called with too short a source.

int main()
{
  char src[] = "aaaaaaa";
  char abc[100];
  char dst[100];
  memccpy(dst, src, 'b', 11);
  return 0;
}
