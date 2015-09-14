/* Double free a structure by having a pointer to itself in a union in
 * the structure. */

#include <stdlib.h>

struct A
{
  union {
    struct A *a;
    int z;
  } U;
  int y;
};

void f(struct A *i)
{
  free(i->U.a);
}

int main()
{
  struct A *a;
  a = malloc(sizeof(struct A));
  a->U.a = a;
  a->y = 100;
  f(a);
  free(a);
  return 0;
}
