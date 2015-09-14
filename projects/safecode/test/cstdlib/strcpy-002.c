// RUN: test.sh -e -t %t %s
#include <string.h>

// strcpy() with object overlap.

int main()
{
  char obj[5] = "1234";
  strcpy(&obj[1], &obj[0]);
  return 0;
}
