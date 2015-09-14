// RUN: test.sh -e -t %t %s

// The destination string is written to out of bounds.

#include <string.h>

int main()
{
  char a[10];
  char b[500];
  a[0] = '\0';
  memset(b, 'b', 10);
  strncat(a, b, 50);
  return 0;
}
