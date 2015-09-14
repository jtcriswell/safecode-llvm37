// RUN: test.sh -e -t %t %s

// strstr() with an unterminated substring.

#include <string.h>

int main()
{
  char substring[] = { 'a', 'b', 'c' };
  char string[] = "abcdefg";

  strstr(string, substring);
  return 0;
}
