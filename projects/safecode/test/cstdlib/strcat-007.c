// RUN: test.sh -e -t %t %s

// Concatenate onto a destination that is too short by one.

#include <string.h>

int main()
{
  char a[11] = "the \0string";
  char b[] = "strings";
  strcat(a, b);
  return 0;
}
