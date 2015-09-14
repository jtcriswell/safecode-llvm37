// RUN: test.sh -p -t %t %s
//
// No fmemopen() on darwin.
// XFAIL: darwin

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// fread() test on EOF conditions.

int main()
{
  uint64_t arr1[32];
  uint64_t arr2[64];
  char buf[32 * sizeof(uint64_t)];
  unsigned i;
  size_t result;
  FILE *f;

  memset(arr1, 0, sizeof(arr1));

  f = fmemopen(buf, sizeof(buf), "r+");
  fwrite(arr1, sizeof(uint64_t), 32, f);
  rewind(f);
  result = fread(arr2, sizeof(uint64_t), 64, f);
  assert(result == 32);
  fclose(f);

  return 0;
}
