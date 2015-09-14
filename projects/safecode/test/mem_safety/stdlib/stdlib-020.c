/* memcpy() in multidimensional arrays */

#include <string.h>
#include <stdio.h>

int main()
{
  char buf[2][10];
  strcpy(buf[0], "String");
  memcpy(buf[1], buf[0], sizeof(buf[0]) + 1);
  printf("%s %s\n", buf[0], buf[1]);
  return 0;
}
