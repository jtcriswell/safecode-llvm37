// RUN: test.sh -e -t %t %s

// strcasestr() on unterminated superstring.

#include <stdlib.h>

extern char *strcasestr(const char *s1, const char *s2);

int main()
{
  char m[100];
  char *s1 = malloc(100);
  memset(m, 'm', 100);
  strcpy(s1, "MEOW");
  strcasestr(m, s1);
  free(s1);
  return 0;
}
