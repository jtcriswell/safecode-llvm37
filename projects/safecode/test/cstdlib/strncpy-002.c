// RUN: test.sh -e -t %t %s
#include <string.h>

// strncpy() with overlapping objects.

int main()
{
  char src[10] = "A string";
  strncpy(&src[5], &src[0], 10);
  return 0;
}
