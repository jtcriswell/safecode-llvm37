/* Unitialized variable in returned structure. */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  union {
    char *ptr;
    int value;
  } items;
  int value;
} Xstruct;

Xstruct *newXstruct()
{
  return malloc(sizeof(Xstruct));
}

int main()
{
  Xstruct *x;
  x = newXstruct();
  printf("%p\n", strstr(x->items.ptr, "string"));
  free(x);
  return 0;
}
