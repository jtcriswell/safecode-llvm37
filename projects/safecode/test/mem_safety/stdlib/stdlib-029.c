/* memmove() with invalid size */

#include <string.h>

int main()
{
  char buf[1000];
  memmove(buf, buf, 2000);
  return 0;
}
