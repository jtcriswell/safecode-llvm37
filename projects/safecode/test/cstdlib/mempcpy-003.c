// RUN: test.sh -p -t %t %s
// XFAIL: darwin
#define _GNU_SOURCE
#include <string.h>
#include <assert.h>

// Example of correct usage of mempcpy().

int main()
{
  int dest[5];
  int source[5] = { 1, 2, 3, 4, 5 };
  void *result;
  result = mempcpy(&dest[0], &source[0], sizeof(int) * 5);
  assert(result == &dest[5]);
  return 0;
}
