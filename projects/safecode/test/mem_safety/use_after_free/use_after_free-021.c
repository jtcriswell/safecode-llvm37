/* Accessing valid memory through a free'd pointer */

#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

typedef struct
{
  char *mem;
} test;

int main()
{
  jmp_buf buf;
  test *t;
  t = malloc(sizeof(test));
  t->mem = malloc(1000);
  if (setjmp(buf) != 0)
    strcpy(t->mem, "String");
  else
  {
    free(t);
    longjmp(buf, 1);
  }
  return 0;
}
