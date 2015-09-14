// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// Example of the correct usage of memcpy().

int main()
{
  char src[] = "aaaaaaaaaab";
  char dst[100];
  memcpy(dst, src, 11);
  assert(memcmp(dst, src, 11) == 0);
  return 0;
}
