/* strcat() with negative pointer index */

#include <string.h>
#include <stdlib.h>

int main()
{
  char *c, *x;
  c = malloc(10);
  c[0] = '\0';
  x = &c[2];
  strcat(&x[-2], "This is more than 10");
  free(c);
  return 0;
}
