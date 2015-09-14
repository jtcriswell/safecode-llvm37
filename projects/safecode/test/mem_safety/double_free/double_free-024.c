/* Double free a pointer by calling the same function (f1()) twice,
   first directly then from f2() .*/
#include <stdlib.h>

void *ptr;

void f1()
{
  free(ptr);
}

void f2()
{
  f1();
}

int main()
{
  ptr = malloc(100);
  f1();
  f2();
  return 0;
}
