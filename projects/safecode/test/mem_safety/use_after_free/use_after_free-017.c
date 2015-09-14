/* Use longjmp() to access a string after it has been free'd. */

#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void f(char *string)
{
  char *ptr = strstr(string, "string");
  jmp_buf buf;
  if (setjmp(buf) != 0)
    strcpy(ptr, "freed string");
  else
  {
    free(string);
    longjmp(buf, 1);
  }
}

int main()
{
  char *ptr;
  ptr = malloc(100);
  strcpy(ptr, "a string");
  f(ptr);
  printf("%s\n", ptr);
  return 0;
}
