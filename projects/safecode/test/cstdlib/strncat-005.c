// RUN: test.sh -e -t %t %s

// strncat() when the source array overlaps with the destination
// string, and nul is not one of the n characters to concatenate.

#include <string.h>

int main()
{
  char string[20] = "a string";
  strncat(string, &string[2], 4);
  return 0;
}
