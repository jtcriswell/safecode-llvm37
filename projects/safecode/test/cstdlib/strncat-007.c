// RUN: test.sh -e -t %t %s

// strncat() where the source array overlaps with the destination, the
// source array begins before the destination, and nul is not one of the
// n characters to concatenate.

#include <string.h>

int main()
{
  char string[20] = "a string";
  strncat(&string[4], &string[1], 5);
  return 0;
}
