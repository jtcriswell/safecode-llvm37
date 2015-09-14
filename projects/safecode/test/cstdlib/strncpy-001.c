// RUN: test.sh -e -t %t %s
#include <string.h>

// strncpy() with too short a destination.

int main()
{
  char dst[10];
  char pad[100];
  char src[] = "A string";
  strncpy(&dst[0], &src[0], 11);
  return 0;
}
