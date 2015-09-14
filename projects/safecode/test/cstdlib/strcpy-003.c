// RUN: test.sh -p -t %t %s

#include <string.h>
#include <assert.h>

// Correct usage of strcpy().

int main()
{
  char buf[10] = "buf";
  char good[8] = "buf\0buf\0";
  char *result;
  result = strcpy(&buf[4], &buf[0]);
  assert(memcmp(&buf[0], &good[0], 8) == 0);
  return 0;
}
