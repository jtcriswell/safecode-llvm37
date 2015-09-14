// RUN: test.sh -e -t %t %s

// This is an example of strncasecmp() reading out of bounds.

#include <strings.h>

int main(void) {
  char str1[4] = { 'S', 't', 'R', '1' };
  char str2[5] = "str1";

  strncasecmp(str1, str2, 5);

  return 0;
}
