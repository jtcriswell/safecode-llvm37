/* sscanf() out of bounds */

#include <stdio.h>
#include <string.h>

void poor_mans_strcpy(char *dst, char *src)
{
  sscanf(dst, "%s", src);
}

int main()
{
  char buf1[50], buf2[100];
  memset(buf2, 'a', 99);
  buf2[99] = '\0';
  poor_mans_strcpy(buf1, buf2);
  return 0;
}
