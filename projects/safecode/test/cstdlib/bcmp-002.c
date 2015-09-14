// RUN: test.sh -p -t %t %s

// This is an example of the correct usage of bcmp().

#include <assert.h>
#include <strings.h>

int main()
{
  char s[10] = "string";
  char t[10] = "string";
  assert(bcmp(s, t, 7) == 0);
  return 0;
}
