// RUN: test.sh -p -t %t %s

// strncat() where the source array does not overlap with the destination
// string but they share the same terminator. This should pass, since the
// items to be copied do not overlap.

#include <string.h>
#include <assert.h>

int main()
{
  char string[20] = "a string:";
  strncat(&string[1], string, 1);
  /* Make sure the function worked as expected. */
  assert(strcmp(string, "a string:a") == 0);
  return 0;
}
