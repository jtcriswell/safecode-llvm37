/* Uninitialized pointer in allocated union. */

#include <stdlib.h>
#include <stdio.h>

typedef union
{
  char *ptr;
  int u;
} example;

int main()
{
  example *e;
  e = malloc(sizeof(example));
  printf("%i\n", *e->ptr);
  free(e);
  return 0;
}
