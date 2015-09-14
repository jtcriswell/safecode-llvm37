/* A large number of %i's */

#include <stdio.h>
#include <string.h>

int main()
{
  char buf[1001];
  char dest[10000];
  int amt;
  amt = buf[0] = 0;
  while (amt++ < 500)
    strcat(buf, "%i");
  snprintf(dest, 10000, buf);
  return 0;
}
