/* vfprintf() with too few arguments */

#define BUFLEN 1000

#include <stdio.h>
#include <stdarg.h>

char buffer[BUFLEN];

void off_by_one(FILE *output, ...)
{
  va_list args;
  va_start(args, output);
  vsprintf(buffer, "Ptr: %n %p %i\n", args);
  va_end(args);
  fprintf(output, "logged\n");
}

int main()
{
  int m;
  off_by_one(stdout, &m, &m, 1);
  off_by_one(stdout, &m, 1);
  return 0;
}
