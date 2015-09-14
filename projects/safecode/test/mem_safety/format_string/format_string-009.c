/* f() is intended to format the printing of an integer
   with user supplied formatting information, but it is unsafe... */
#include <stdio.h>

void f(char *number_string, int item)
{
  char buffer[100];
  sprintf(buffer, "%%%si\n", number_string);
  printf(buffer, item);
}

int main()
{
  f("030", 1000);
  f("%%i%s", 999);
  return 0;
}
