// RUN: test.sh -e -t %t %s
#include <string.h>

// strcpy() with too short a destination.

int main()
{
  char dst[4];
  char pad[100];
  char src[] = "12345";
  strcpy(&dst[0], &src[0]);
  return 0;
}
