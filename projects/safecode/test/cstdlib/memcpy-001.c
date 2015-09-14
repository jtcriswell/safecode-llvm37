// RUN: test.sh -e -t %t %s
#include <string.h>

// memcpy() called with too short a destination.

int main(int argc, char ** argv)
{
  char src[] = "aaaaaaaaaab";
  char dst[10];
  memcpy(dst, src, 11);
  printf ("%c\n", dst[argc]);
  return 0;
}
