// RUN: test.sh -p -t %t %s

#include <assert.h>
#include <string.h>

// Example of the correct usage of strspn().

int main()
{
  char *str = "A string";
  size_t result;
  result = strspn(str, str);
  assert(result = 8);
  return 0;
}
