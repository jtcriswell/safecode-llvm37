// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>

// The correct usage of strncpy().

int main()
{
  char source[] = "source";
  char dest[10] = "123456789";
  char old[10]  = "123456789";
  char good[10] = "source";
  strncpy(&dest[0], &source[0], 10);
  assert(memcmp(&dest[0], &good[0], 10) == 0);
  assert(memcmp(&dest[0], &old[0], 10)  != 0);
  return 0;
}
