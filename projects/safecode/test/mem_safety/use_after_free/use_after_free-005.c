/* f() is given a string. It first interprets the string
   as a pointer to a pointer to a string. Then it replaces
   the first character of the string with the first byte
   of the string that the initial bytes pointed to.
   f() is used to cause a use-after-free when the
   string its argument references is no longer allocated. */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void f(char *t)
{
  printf("first item in t is %p\n", ((char**)t)[0]);
  t[0] = ((char**) t)[0][0];
}

int main()
{
  char *t, *s;

  t = malloc(sizeof(char) * 400);
  s = malloc(sizeof(char) * 400);
  s[0] = 'm';
  printf("s is %p\n", s);
  free(s);
  memcpy(t, &s, sizeof(char*));
  f(t);
  free(t);
  return 0;
}
