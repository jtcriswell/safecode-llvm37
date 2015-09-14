// RUN: test.sh -p -t %t %s
// XFAIL: darwin
#include <string.h>
#include <assert.h>

// Example of the correct usage of strnlen().

int main()
{
  char string[10] = "A string";
  size_t result;
  result = strnlen(string, 100);
  assert(result == 8);
  return 0;
}
