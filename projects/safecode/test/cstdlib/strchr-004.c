// RUN: test.sh -p -t %t %s

// This is an example of the correct usage of strchr().

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  char *str = malloc(100);
  strcpy(str, "arma virumque cano");
  char *pos = strchr(&str[3], 'a');
  assert(pos == &str[3]);
  free(str);
  return 0;
}
