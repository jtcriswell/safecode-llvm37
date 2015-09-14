// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>
#include <locale.h>

// Example of the correct usage of strxfrm().

int main()
{
  char dst[10];
  char *src = "A string.";
  setlocale(LC_ALL, "C");
  size_t sz;
  sz = strxfrm(dst, src, 10);
  assert(sz == 9);
  assert(memcmp(src, dst, 10) == 0);
  return 0;
}
