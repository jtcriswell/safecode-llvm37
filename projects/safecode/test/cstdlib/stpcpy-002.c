// RUN: test.sh -e -t %t %s

// stpcpy() with an unterminated source string.

#define _GNU_SOURCE
#include <string.h>

int main()
{
  char src[100];
  char dst[1000];
  memset(src, 'a', 100);
  stpcpy(dst, src);
  return 0;
}
