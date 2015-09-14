// RUN: test.sh -e -t %t %s

// strncat() reads out of bounds from the source string.

#include <string.h>

int main()
{
  char a[1000];
  char b[100];
  a[0] = '\0';
  memset(b, 'b', 100);
  strncat(a, b, 1000);
  return 0;
}
