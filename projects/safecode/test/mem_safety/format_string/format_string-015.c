/* lots of %n's */

#include <stdio.h>
#include <string.h>

int main()
{
  char buf[101];
  int amt;
  amt = buf[0] = 0;
  while (amt++ < 50)
    strcat(buf, "%n");
  printf(buf);
  return 0;
}
