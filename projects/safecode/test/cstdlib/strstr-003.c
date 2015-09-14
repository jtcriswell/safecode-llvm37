// RUN: test.sh -p -t %t %s

// This is an example of the correct usage of strstr().

#include <string.h>
#include <stdlib.h>
#include <assert.h>

int main()
{
  char *s1 = malloc(100);
  char *s2 = malloc(100);
  strcpy(s1, "arma virumque cano");
  strcpy(s2, "que");
  char *substr = strstr(&s1[4], s2);
  assert(substr == &s1[10]);
  free(s1);
  free(s2);
  return 0;
}
