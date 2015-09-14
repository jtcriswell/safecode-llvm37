/* strncat() with small buffer */

#include <string.h>

int main()
{
  char buf[1];
  buf[0] = '\0';
  strncat(buf, "ab", 2);
  return 0;
}
