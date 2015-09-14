// RUN: test.sh -p -t %t %s

// This is an example of the correct usage of stpcpy().

#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  char *str = malloc(100);
  char *cpy = stpcpy(str, "arma virumque cano");
  assert(cpy == &str[18]);
  free(str);
  return 0;
}
