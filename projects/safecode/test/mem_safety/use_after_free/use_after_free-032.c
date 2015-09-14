/* Return a free'd pointer from a jump sequence. */

#include <setjmp.h>
#include <stdlib.h>

jmp_buf buffer;

char *f()
{
  volatile char *ptr;
  if (setjmp(buffer) != 0)
  {
    free((void *) ptr);
    return (char *) ptr;
  }
  ptr = malloc(1000);
  longjmp(buffer, 1);
}

int main()
{
  *f() = ' ';
  return 0;
}
