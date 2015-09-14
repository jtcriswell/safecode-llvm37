/* calloc() and free via realloc(), via function pointers */

#include <stdlib.h>

typedef void *(*cptr)(size_t, size_t);
typedef void *(*rptr)(void *, size_t);

int main()
{
  cptr c;
  rptr r;
  int *i;

  c = calloc;
  r = realloc;

  i = c(1, sizeof(int));
  r(i, 0);
  *i = 99;

  return 0;
}
