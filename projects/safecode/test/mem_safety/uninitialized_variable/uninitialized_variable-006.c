/* Uninitialized union contents in jump. */

#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>

typedef union {
  char *ptr1;
  int value;
} test;


int main()
{
  test *t = malloc(sizeof(test));
  jmp_buf buf;
  if (setjmp(buf) != 0)
  {
    printf("%s\n", t->ptr1);
    free(t);
    return 0;
  }
  longjmp(buf, 1);
}
