// RUN: test.sh -p -t %t %s

// Test to see that strncasecmp() works as expected.

#include <assert.h>
#include <strings.h>

int main(void) {
  assert(strncasecmp("abc", "aDc", 3) < 0);
  assert(strncasecmp("aBc", "adc", 3) < 0);
  assert(strncasecmp("xzZ", "xzz", 3) == 0);
  assert(strncasecmp("xzz", "xzZ", 3) == 0);
  assert(strncasecmp("XZz", "xyz", 3) > 0);
  assert(strncasecmp("Xzz", "xYz", 3) > 0);
  return 0;
}
