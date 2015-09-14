// RUN: test.sh -c -p -t %t %s

// This is an example of the correct usage of strrchr().

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  char *str = malloc(100);
  strcpy(str, "arma virumque cano");
  char *pos = strrchr(&str[3], 'a');
  assert(pos == &str[15]);
  free(str);
  return 0;
}
