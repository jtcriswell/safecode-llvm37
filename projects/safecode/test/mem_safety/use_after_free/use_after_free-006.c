/* Return the dereferenced value of a pointer
   that has already been freed (use after free) */
#include <stdlib.h>
#include <stdint.h>

int main()
{
  char *c, *d;
  c = malloc(sizeof(char));
  d = malloc(sizeof(char));
  free(c);
  d[0] = '\0';
  free(d);
  return d[0];
}
