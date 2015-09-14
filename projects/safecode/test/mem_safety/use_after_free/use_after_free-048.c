/* realloc() and free() via function pointers */

#include <stdlib.h>

typedef void *(*rptr)(void *, size_t);
typedef void (*fptr)(void *);

int main()
{
  rptr r;
  fptr f;
  int *i;

  r = realloc;
  f = free;

  i = r(NULL, sizeof(int));
  f(i);
  *i = 99;
  return 0;
}
