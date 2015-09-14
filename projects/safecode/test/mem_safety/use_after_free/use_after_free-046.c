/* Allocating and freeing memory using function pointers. */

#include <stdlib.h>

typedef void *(*mptr)(size_t);
typedef void  (*fptr)(void *);

int main()
{
  mptr m;
  fptr f;
  int *i;

  m = malloc;
  f = free;

  i = m(sizeof(int));
  f(i);
  *i = 99;

  return 0;
}
