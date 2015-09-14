// RUN: test.sh -e -t %t %s

// strcspn() reading past end of the input string.

#include <string.h>

int main()
{
  char input[100];
  memset(input, 'a', 100);
  size_t result;
  result = strspn(input, "ghijk");
  return 0;
}
