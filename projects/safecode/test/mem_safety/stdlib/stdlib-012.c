/* strcat() with not enough destination space in an allocated array */

#include <string.h>
#include <stdlib.h>

int main()
{
  char *c, *x;
  c = malloc(30);
  x = &c[25];
  x[0] = '\0';
  strcat(x, "String");
  free(c);
  return 0;
}
