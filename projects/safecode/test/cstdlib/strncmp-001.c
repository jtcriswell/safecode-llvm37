// RUN: test.sh -e -t %t %s
#include <string.h>

// strncmp() with one object too short.

int main()
{
  char str1[]  = "str1str1str1";
  char str2[4] = { 's', 't', 'r', '1' };
  int result;
  result = strncmp(&str1[0], &str2[0], 5);
  return 0;
}
