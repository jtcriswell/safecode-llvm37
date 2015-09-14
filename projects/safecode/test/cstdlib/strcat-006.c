// RUN: test.sh -e -t %t %s

// strcat() onto the end of an object that is not nul terminated,
// but the front of the object is.

#include <string.h>

int main()
{
  char dst[100] = "";
  memset(&dst[1], 'a', 99);
  strcat(&dst[1], "string");
  return 0;
}
