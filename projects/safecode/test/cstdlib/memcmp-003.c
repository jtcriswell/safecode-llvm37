// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// Example of the correct usage of memcmp().

int main()
{
  char *string1 = "This is a string.";
  char *string2 = "This is a s\tring.";
  int result;
  result = memcmp(&string1[2], &string2[2], 10);
  assert(result > 0);
  return 0;
}
