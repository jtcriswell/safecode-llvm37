// RUN: test.sh -p -t %t %s

// This is an example of the correct usage of strcasestr().

#include <stdlib.h>
#include <assert.h>

extern char *strcpy(char *dst, const char *src);
extern char *strcasestr(const char *s1, const char *s2);

int main()
{
  char *s1 = malloc(100);
  char *s2 = malloc(100);
  strcpy(s1, "arma virumque cano");
  strcpy(s2, "QUE");
  char *substr = strcasestr(&s1[4], s2);
  assert(substr == &s1[10]);
  free(s1);
  free(s2);
  return 0;
}
