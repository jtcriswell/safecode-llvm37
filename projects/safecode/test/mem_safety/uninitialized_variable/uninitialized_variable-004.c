/* Uninitialized variable in an allocated struct. */

#include <stdlib.h>
#include <stdio.h>

struct X {
  struct X *x;
  int u;
};

int main()
{
  struct X *ptr;
  ptr = malloc(sizeof(struct X));
  printf("%i\n", ptr->x->u);
  free(ptr);
  return 0;
}
