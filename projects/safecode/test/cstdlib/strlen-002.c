// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// Correct usage of strlen().

int main()
{
  char string[] = "string";
  assert(strlen(string) == 6);
  return 0;
}
