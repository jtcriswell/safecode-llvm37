/* snprintf() with insufficient arguments. */

#include <string.h>
#include <stdio.h>

int main()
{
  char buf[30];
  snprintf(buf, 30, "%i %i %n");
  return 0;
}
