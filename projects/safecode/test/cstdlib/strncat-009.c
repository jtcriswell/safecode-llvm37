// RUN: test.sh -e -t %t %s

// strncat() when the source array contains the whole destination string.

#include <string.h>

int main()
{
  char string[100] = "This is\0 a string.";
  strncat(&string[4], &string[1], 20);
  return 0;
}
