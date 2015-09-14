// RUN: test.sh -e -t %t %s
//
// No fmemopen() on darwin.
// XFAIL: darwin

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// fgets() buffer overflow.

int main()
{
  char buf[1024];
  char str[10];
  FILE *f;

  f = fmemopen(buf, sizeof(buf), "r+");
  fputs("fputs() wrote this\n", f);
  rewind(f);
  fgets(str, 20, f);
  fclose(f);

  return 0;
}
