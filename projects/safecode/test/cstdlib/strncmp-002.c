// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// The correct usage of strncmp().

int main()
{
  char str1[]  = "str1str1str1";
  char str2[4] = { 's', 't', 'r', '2' };
  int result;
  result = strncmp(&str1[0], &str2[0], 4);
  assert(result < 0);
  return 0;
}
