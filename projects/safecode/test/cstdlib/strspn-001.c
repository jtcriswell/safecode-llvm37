// RUN: test.sh -e -t %t %s

// strspn() reading past end of the input string.

#include <string.h>

int main()
{
  char input[100];
  memset(input, 'a', 100);
  size_t result;
  result = strspn(input, "bcda");
  return 0;
}
