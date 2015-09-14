/* Allocate a pointer to a function. Call that
   pointer after it has already been freed. */

#include <stdlib.h>

int f()
{
  return 6;
}

int main()
{
  int (*(*p))();
  p = malloc(sizeof(int (*)()));
  *p = f;
  free(p);
  return (*p)();
}
