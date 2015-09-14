/* Use of a function pointer embedded in a free'd union. */

#include <stdio.h>
#include <stdlib.h>

typedef int (*pfptr)(const char *, ...);

union test
{
  pfptr func;
  int value;
};

int main()
{
  union test *t;

  t = malloc(sizeof(union test) * 10);
  t[0].value = 1000;
  t[8].func = printf;
  free(t);
  t[8].func("hello world\n");
  return 0;
}
