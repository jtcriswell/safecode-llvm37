/* Double free a string after returning to positiong with
   longjmp() */
#include <setjmp.h>
#include <stdlib.h>

void f1(char *string);
void f2(char *string);

jmp_buf buf;

void f1(char *string)
{
  if (setjmp(buf) != 0)
    free(string);
  else
    f2(string);
}

void f2(char *string)
{
  free(string);
  longjmp(buf, 1);
}

int main()
{
  char *string = malloc(1000);
  f1(string);
  return 0;
}
