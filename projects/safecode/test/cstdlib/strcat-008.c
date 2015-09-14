// RUN: test.sh -e -t %t %s

// strcat() with nothing copied although strings overlap.

#include <string.h>

int main()
{
  char string[7] = "string";
  strcat(string, &string[6]);
  return 0;
}
