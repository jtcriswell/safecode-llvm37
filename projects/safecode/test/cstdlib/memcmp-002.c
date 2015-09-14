// RUN: test.sh -e -t %t %s
#include <string.h>

// memcmp() past the end of one memory object but not the another.

int main()
{
  char a[10] = "abcdefg";
  char b[20] = "abcdefg";
  int result;
  result = memcmp(&a[1], &b[1], 10);
  return 0;
}
