// RUN: test.sh -e -t %t %s
// XFAIL: darwin

#define _GNU_SOURCE
#include <string.h>

// mempcpy() with too short a destination.

int main()
{
  int dest[5];
  int source[5] = { 1, 2, 3, 4, 5 };
  void *result;
  result = mempcpy(&dest[1], &source[0], sizeof(int) * 5);
  return 0;
}
