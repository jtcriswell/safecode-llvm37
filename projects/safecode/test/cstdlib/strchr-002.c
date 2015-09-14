// RUN: test.sh -e -t %t %s

// Call strchr() on an unterminated string, and the character to find
// is not inside the string.

#include <string.h>

int main()
{
  char a[1000];
  memset(a, 'a', 1000);
  strchr(a, 'b');
  return 0;
}
