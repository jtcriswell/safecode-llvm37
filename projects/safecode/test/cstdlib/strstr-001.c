// RUN: test.sh -e -t %t %s

// strstr() on unterminated superstring.

#include <string.h>
#include <stdlib.h>

int main()
{
  char m[100];
  char *s1 = malloc(100);
  memset(m, 'm', 100);
  strcpy(s1, "meow");
  strstr(m, s1);
  free(s1);
  return 0;
}
