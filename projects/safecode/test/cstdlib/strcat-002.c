// RUN: test.sh -e -t %t %s

// Try concatenating from an unterminated source string. This should
// fail.

#include <stdlib.h>
#include <string.h>

void do_cat(char *src)
{
  char f[100] = "meow";
  strcat(f, src);
}

int main()
{
  char *n = malloc(10);
  strcpy(n, "meow");
  memset(&n[4], 'a', 6);

  do_cat(n);
  free(n);
  return 0;
}
