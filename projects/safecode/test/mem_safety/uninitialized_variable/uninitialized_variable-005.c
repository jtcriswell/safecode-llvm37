/* Uninitialized variable with setjmp() */

#include <setjmp.h>
#include <string.h>

typedef struct
{
  char *ptr1;
  int value;
} test;

void f();
void g(jmp_buf b);

void f()
{
  test t;
  jmp_buf b;
  t.value = 1000;
  if (setjmp(b) != 0)
  {
    strcpy(t.ptr1, "String");
    return;
  }
  else
    g(b);
}

void g(jmp_buf b)
{
  longjmp(b, 1);
}

int main()
{
  f();
  return 0;
}
