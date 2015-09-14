/* Format string error while
   passing around va_list before it is sent to vsprintf() */

#include <stdio.h>
#include <stdarg.h>

void g(va_list v, char *dest);

void f(char *dest, ...)
{
  va_list args;
  va_start(args, dest);
  g(args, dest);
  va_end(args);
}

void g(va_list v, char *dest)
{
  vsprintf(dest, "%i %n %s", v);
  printf("%s\n", dest);
}

int main()
{
  char buffer[100];
  int amt;
  f(buffer, 100, &amt);
  return 0;
}
