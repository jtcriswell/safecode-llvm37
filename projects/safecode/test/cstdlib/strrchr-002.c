// RUN: test.sh -c -e -t %t %s

// strrchr() on an unterminated string, with the character being searched
// for not found in the string.

#include <string.h>

int main()
{
  char a[1000];
  memset(a, 'a', 1000);
  strrchr(a, 'b');
  return 0;
}
