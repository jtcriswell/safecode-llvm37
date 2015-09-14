// RUN: test.sh -p -t %t %s

#include <assert.h>
#include <string.h>

// Example of the correct usage of strcspn().

int main()
{
  char *str = "A string";
  size_t result;
  result = strspn(str, "B");
  assert(result = 8);
  return 0;
}
