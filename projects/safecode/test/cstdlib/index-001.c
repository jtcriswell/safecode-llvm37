// RUN: test.sh -e -t %t %s

// Call index() on an unterminated string, and the character
// to find is not inside the string.

#include <strings.h>

int main()
{
  char a[1000];
  memset(a, 'a', 1000);
  index(a, ' ');
  return 0;
}
