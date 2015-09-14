// RUN: test.sh -e -t %t %s
#include <string.h>

// memccpy() called with too short a destination.

int main()
{
  char src[] = "aaaaaaaaaab";
  char abc[100];
  char dst[10];
  memccpy(dst, src, 'b', 11);
  return 0;
}
