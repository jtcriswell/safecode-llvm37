// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// Example of the correct usage of memccpy().

int main()
{
  char src[] = "aaaaaaaaaab";
  char abc[100];
  char dst[100];
  memccpy(dst, src, 'b', 11);
  assert(memcmp(dst, src, 11) == 0);
  return 0;
}
