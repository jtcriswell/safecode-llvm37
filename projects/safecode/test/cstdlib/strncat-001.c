// RUN: test.sh -e -t %t %s

// This is an example of a destination string too short for strncat().

#include <string.h>

int main()
{
  char a[10];
  char b[100];
  memset(a, 'a', 9);
  a[9] = '\0';
  memset(b, 'b', 100);
  strncat(a, b, 1);
  return 0;
}
