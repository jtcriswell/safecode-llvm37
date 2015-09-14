// RUN: test.sh -p -t %t %s

// This is an example of the correct usage of strpbrk().

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main()
{
  char *str  = malloc(100);
  char *chrs = malloc(100);
  strcpy(str, "urbs antiqua fuit");
  strcpy(chrs, "uaqf");
  char *pos = strpbrk(&str[1], chrs);
  assert(pos == &str[5]);
  free(str);
  free(chrs);
  return 0;
}
