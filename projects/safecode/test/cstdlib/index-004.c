// RUN: test.sh -p -t %t %s

// This is an example of the correct usage of index().

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

int main()
{
  char *str = malloc(100);
  strcpy(str, "arma virumque cano");
  char *pos = index(&str[3], 'a');
  assert(pos == &str[3]);
  free(str);
  return 0;
}
