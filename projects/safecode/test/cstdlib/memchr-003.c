// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// This is an example of the correct usage of memchr().

int main()
{
  char c[8] = "ABCADEF";
  char *pt;
  pt = memchr(&c[1], 'A', 4);
  assert(pt == &c[3]);
  return 0;
}
