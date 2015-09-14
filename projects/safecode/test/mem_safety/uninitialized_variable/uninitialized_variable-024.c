/* Using a pointer to an uninitialized variable. */

#include <string.h>

void g(char **ptr)
{
  strcpy(*ptr, "String");
}

void f()
{
  char *p;
  g(&p);
}

int main()
{
  f();
  return 0;
}
