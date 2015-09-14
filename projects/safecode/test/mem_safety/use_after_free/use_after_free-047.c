/* calloc() and free() using function pointers */

#include <stdlib.h>

typedef void *(*cptr)(size_t, size_t);
typedef void (*fptr)(void *);

int main()
{
  cptr c;
  fptr f;
  int *i;

  c = calloc;
  f = free;

  i = c(1, sizeof(int));
  f(i);
  *i = 99;
  return 0;
}
