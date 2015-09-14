/* Read from an uninitialized string. */

#include <string.h>

int main()
{
  char *string;
  char buf[1000];
  memcpy(buf, string, 1000);
  return 0;
}
