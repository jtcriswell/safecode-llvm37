/* Try to dereference a pointer from a union
   that has already been freed. */
#include <stdlib.h>

typedef union
{
  char *cptr;
  int *iptr;
  int val;
} U;

int main()
{
  int x;
  U *u;

  u = malloc(sizeof(U));
  u->val = 500;
  u->iptr = &x;
  free(u);
  *(u->iptr) = 5;
  return 0;
}
