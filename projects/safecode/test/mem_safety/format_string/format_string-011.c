/* vfprintf() error with jumps */

#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>

jmp_buf buf;

void setup_log(FILE *output)
{
  fflush(output);
  longjmp(buf, 1);
}

void dolog(FILE *output, ...)
{
  va_list args;
  va_start(args, output);
  if (setjmp(buf) != 0)
  {
    vfprintf(output, "%i %i %n\n", args);
    va_end(args);
  }
  else
    setup_log(output);
}

int main()
{
  int ptr;
  dolog(stdout, 3, 4, &ptr);
  dolog(stdout, 1, 2);
  return 0;
}
