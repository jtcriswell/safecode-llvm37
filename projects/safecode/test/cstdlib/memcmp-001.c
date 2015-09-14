// RUN: test.sh -e -t %t %s
#include <string.h>

// memcmp() past the end of a memory object.

int main()
{
  char c[] = "abcdef";
  char *d  = &c[0];
  memcmp(&c[0], d, 20);
  return 0;
}
