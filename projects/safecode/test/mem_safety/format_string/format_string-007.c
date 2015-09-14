/* vfprintf() with improper argument amount */

#include <stdarg.h>
#include <stdio.h>

void output(FILE *output, ...)
{
  va_list args;
  va_start(args, output);
  vfprintf(output, "%i %i %n\n", args);
  va_end(args);
}

int main()
{
  output(stdout, 31, 200);
  return 0;
}
