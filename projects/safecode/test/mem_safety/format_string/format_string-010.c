/* vsscanf() with insufficient arguments */

#include <stdio.h>
#include <stdarg.h>

void set(char *string, ...)
{
  va_list args;
  va_start(args, string);
  vsscanf("123 string", string, args);
  va_end(args);
}


int main()
{
  char buffer[100];
  int  amt;
  set("%i %s %n", &amt, &buffer);
  printf("%i %s\n", amt, buffer);
  return 0;
}
