/* Use after free of pointer a1 which is passed into p which frees
   the pointer only if it dereferences into a nonzero value. */

#include <stdlib.h>

char *p(char *ptr)
{
  if (*ptr)
    free(ptr);
  return ptr;
}

int main()
{
  char *a1, *a2;
  a1 = malloc(100);
  a2 = malloc(100);
  *a1 = 'c';
  *a2 = 0;
  a1 = p(a1);
  a2 = p(a2);
  *a1 = 'a';
  *a2 = 'b';
  free(a2);
  return 0;
}
