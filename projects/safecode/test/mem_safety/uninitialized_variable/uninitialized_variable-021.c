/* Uninitialized variable with jumps. */

#include <stdio.h>
#include <setjmp.h>
#include <string.h>

void g(jmp_buf b)
{
  longjmp(b, 1);
}

void f()
{
  char *ptr;
  jmp_buf buf;
  if (setjmp(buf) != 0)
  {
    strcpy(ptr, "String");
    return;
  }
  g(buf);
}

int main()
{
  f();
  return 0;
}
