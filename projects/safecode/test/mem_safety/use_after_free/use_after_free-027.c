/* Free a pointer to a union that references itself and then use it. */

#include <stdlib.h>
#include <stdio.h>

typedef union _u
{
  union _u *ptr;
  int v;
} s_union;

int main()
{
  s_union *a, *b;
  a = malloc(sizeof(s_union));
  b = malloc(sizeof(s_union));
  a->ptr = b;
  b->ptr = a;
  free(a->ptr->ptr);
  free(b);
  a->v = 100;
  printf("%i\n", a->v);
  return 0;
}
