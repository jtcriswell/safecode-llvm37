// RUN: test.sh -p -t %t %s

// This is an example of the correct usage of rindex().

#include <assert.h>
#include <stdlib.h>
#include <strings.h>

int main()
{
  char *str = malloc(100);
  strcpy(str, "arma virumque cano");
  char *pos = rindex(&str[3], 'a');
  assert(pos == &str[15]);
  free(str);
  return 0;
}
