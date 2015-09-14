/* Use an integer as a pointer in a union */
#include <stdlib.h>

union ab
{
  int a;
  int *b;
};

void f(union ab *p);

void f(union ab *p)
{
  *p->b = 100;
}

int main()
{
  union ab a;
  a.a = 100;
  f(&a);
  return 0;
}
