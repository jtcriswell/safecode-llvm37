// RUN: test.sh -e -t %t %s
#include <string.h>

// strlen() on unterminated string.

int main()
{
  char c[1] = { 's' };
  int result;
  result = strlen(&c[0]);
  return 0;
}
