/* strlen() out of bounds */

#include <string.h>
#include <stdio.h>

int main()
{
  char *string1 = "A Medium String";
  char buf[10];
  strncpy(buf, string1, 9);
  memset(buf, 'a', 10);
  printf("%i\n", strlen(buf));
  return 0;
}
