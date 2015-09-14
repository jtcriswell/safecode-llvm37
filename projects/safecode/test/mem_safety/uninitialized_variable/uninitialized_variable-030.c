/* Global value holds pointer to uninitialized pointer. */

#include <stdio.h>

int **item;

void f()
{
  printf("%i\n", **item);
}

int main()
{
  int *m;
  item = &m;
  f();
  return 0;
}
