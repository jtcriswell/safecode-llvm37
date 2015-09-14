// RUN: test.sh -p -t %t %s
// XFAIL: darwin
#include <string.h>
#include <assert.h>

// Example of the correct usage of strnlen().

int main()
{
  char string[10];
  memcpy(string, "Unterminated string", 10);
  size_t result;
  result = strnlen(string, 10);
  assert(result == 10);
  return 0;
}
