/* sscanf() with too few arguments */

#include <stdio.h>

int main()
{
  char buffer[] = "This is a string.";
  char buf1[100], buf2[100], buf3[100], buf4[100];
  sscanf(buffer, "%s %s %s %s", buf1, buf2, buf3);
  return 0;
}
