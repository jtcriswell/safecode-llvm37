// RUN: test.sh -p -t %t %s
//
// No fmemopen() on darwin.
// XFAIL: darwin

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Ensure that fread() works as expected.

int main()
{
  uint64_t arr1[64];
  uint64_t arr2[64];
  uint64_t one = 1;
  char buf[5000];
  unsigned i;
  size_t result;
  FILE *f;

  for (i = 0; i < 64; ++i) {
    arr1[i] = (one << i);
  }

  f = fmemopen(buf, sizeof(buf), "r+");
  fwrite(arr1, sizeof(uint64_t), 64, f);
  rewind(f);
  result = fread(arr2, sizeof(uint64_t), 64, f);
  assert(result == 64);
  for (i = 0; i < 64; ++i) {
    assert(arr2[i] = (one << i));
  }
  fclose(f);

  return 0;
}
