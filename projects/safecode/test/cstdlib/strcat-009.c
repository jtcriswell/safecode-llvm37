// RUN: test.sh -p -t %t %s

// This is an example of a valid invocation of strcat().

#include <string.h>
#include <assert.h>

int main()
{
  char string1[100] = "string1";
  char string2[100] = "string2";
  strcat(&string1[5], &string2[6]);
  assert(strcmp(string1, "string12") == 0);
  return 0;
}
