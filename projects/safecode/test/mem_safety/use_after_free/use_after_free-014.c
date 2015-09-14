/* Frees the internals of a structure by first making a copy of the structure
   and freeing the pointers contained in the copy. Then attempts to use the
   freed pointers. */

#include <stdlib.h>
#include <string.h>

struct a
{
  int *array;
};

void f(struct a *p)
{
  struct a tmp;
  memcpy(&tmp, p, sizeof(struct a));
  free(tmp.array);
}

int main()
{
  struct a one;
  one.array = malloc(sizeof(int));
  one.array[0] = 1;
  f(&one);
  one.array[0] = 2;
  return 0;
}
