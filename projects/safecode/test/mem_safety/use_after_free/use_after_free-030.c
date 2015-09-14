/* Use after free with jumps */

#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

jmp_buf b;

void f(void *ptr)
{
  free(ptr);
  longjmp(b, 1);
}

int main()
{
  char *ptr;
  ptr = malloc(100);
  if (setjmp(b) != 0)
  {
    strcpy(ptr, "String");
    return 0;
  }
  f(ptr);
}
