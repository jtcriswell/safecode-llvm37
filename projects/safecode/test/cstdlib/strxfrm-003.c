// RUN: test.sh -p -t %t %s
#include <string.h>
#include <assert.h>
#include <locale.h>

// Example of the correct usage of strxfrm().

int main()
{
  char *source = "This is a string";
  setlocale(LC_ALL, "C");
  size_t sz;
  sz = strxfrm(NULL, source, 0);
  assert(sz == 16);
  return 0;
}
