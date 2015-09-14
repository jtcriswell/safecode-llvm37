// RUN: test.sh -e -t %t %s
//
// No fmemopen() on darwin.
// XFAIL: darwin

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// fread() causing a buffer overflow.

int main()
{
  uint64_t arr1[64];
  uint64_t arr2[32];
  uint64_t one = 1;
  char buf[5000];
  unsigned i;
  FILE *f;

  for (i = 0; i < 64; ++i) {
    arr1[i] = (one << i);
  }

  f = fmemopen(buf, sizeof(buf), "r+");
  fwrite(arr1, sizeof(uint64_t), 64, f);
  rewind(f);
  fread(arr2, sizeof(uint64_t), 64, f);
  fclose(f);

  return 0;
}
