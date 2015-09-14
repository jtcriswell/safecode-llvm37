// RUN: test.sh -e -t %t %s
#include <string.h>

// strcoll() reading out of bounds.

int main()
{
  char abc[] = { 'a', 'b', 'c' };
  char strabc[] = "abc";
  int result;
  result = strcoll(&abc[0], &strabc[0]);
  return 0;
}
