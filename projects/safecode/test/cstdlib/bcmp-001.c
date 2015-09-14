// RUN: test.sh -e -t %t %s

// bcmp() with too long value for length of comparison.

#include <strings.h>

int main()
{
  char s[10] = "string";
  char t[10] = "string";
  bcmp(s, t, 100);
  return 0;
}
