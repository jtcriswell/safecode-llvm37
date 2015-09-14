/* snprintf() with incorrect size */

#include <stdio.h>
#include <string.h>

int main()
{
  char buf1[20], buf2[30];
  strcpy(buf2, "This is between 20 and 30");
  snprintf(buf1, 30, buf2);
  return 0;
}
