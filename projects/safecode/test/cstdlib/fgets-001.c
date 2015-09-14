// RUN: test.sh -p -t %t %s
//
// No fmemopen() on darwin.
// XFAIL: darwin

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Ensure that fgets() works as expected.

int main()
{
  char buf[1024];
  char str[100];
  FILE *f;
  char *result;

  f = fmemopen(buf, sizeof(buf), "r+");
  fputs("fputs() wrote this\n", f);
  rewind(f);
  result = fgets(str, sizeof(str), f);
  assert(result == str);
  assert(strcmp(str, "fputs() wrote this\n") == 0);
  fclose(f);

  return 0;
}
