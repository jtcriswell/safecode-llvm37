/* Double free of a calloced pointer. */

#include <stdlib.h>

typedef void (*fptr)(void *);

#define SIZE 50

int main()
{
  fptr *p;

  p = calloc(SIZE, sizeof(fptr));
  p[0] = free;
  p[0](p);
  free(p);
  return 0;
}
