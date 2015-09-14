// RUN: test.sh -e -t %t %s

// rindex() with an unterminated string searching for a character not
// that is not found in the string.

#include <strings.h>

int main()
{
  char a[1000];
  memset(a, 'a', 1000);
  rindex(a, ' ');
  return 0;
}
