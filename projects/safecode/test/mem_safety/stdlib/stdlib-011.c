/* strcat() which eventually overflows the destination buffer. */

#include <string.h>

int main()
{
  char buf[100];
  strcpy(buf, "String");
  while (1)
    strcat(buf, "String");
  return 0;
}
