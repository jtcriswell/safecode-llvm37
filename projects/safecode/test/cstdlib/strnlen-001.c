// RUN: test.sh -e -t %t %s
// XFAIL: darwin
#include <string.h>

// strnlen() reads out of bounds.

int main()
{
  char string[10];
  memcpy(string, "Unterminated string", 10);
  size_t result;
  result = strnlen(string, 11);
  return 0;
}
