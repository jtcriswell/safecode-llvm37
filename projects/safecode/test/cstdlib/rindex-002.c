// RUN: test.sh -e -t %t %s

// rindex() on an unterminated string, with the character being searched
// for not found in the string.

#include <strings.h>

int main()
{
  char a[1000];
  memset(a, 'a', 1000);
  rindex(a, 'X');
  return 0;
}
