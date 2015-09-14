/* strcat() with invalid source */

#include <string.h>

int main()
{
  char *ptr;
  char buf[10];
  buf[0] = '\0';
  strcat(buf, ptr);
  return 0;
}
