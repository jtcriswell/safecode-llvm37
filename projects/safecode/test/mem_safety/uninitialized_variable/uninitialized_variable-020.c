/* Uninitalized variable from nested function calls. */

#include <stdlib.h>
#include <stdio.h>

void *f()
{
  void *ptr;
  return ptr;
}

void *g()
{
  void *ptr1, *ptr2;
  ptr2 = f();
  return ptr1;
}

void *h()
{
  return g();
}

int main()
{
  int *u;
  u = h();
  printf("%i\n", u[10]);
  return 0;
}
