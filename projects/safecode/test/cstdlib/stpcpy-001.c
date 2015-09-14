// RUN: test.sh -e -t %t %s

// stpcpy() with destination buffer too short for source string.
// SAFECode should complain.

#define _GNU_SOURCE
#include <string.h>

int main()
{
  char str1[10];
  char str2[11];
  memset(str2, 'a', 10);
  str2[10] = '\0';
  stpcpy(str1, str2);
  return 0;
}
