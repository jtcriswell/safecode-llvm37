// RUN: test.sh -e -t %t %s
// XFAIL: darwin
#define _GNU_SOURCE
#include <string.h>

// mempcpy() with too short a source.

int main()
{
  int dest[6];
  int source[5] = { 1, 2, 3, 4, 5 };
  void *result;
  result = mempcpy(&dest[0], &source[0], sizeof(int) * 6);
  return 0;
}
