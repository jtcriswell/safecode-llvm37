// RUN: test.sh -e -t %t %s
#include <string.h>
#include <locale.h>

// strxfrm() with too short a destination.

int main()
{
  char dst[9];
  char *source = "This is a string";
  setlocale(LC_ALL, "C");
  size_t sz;
  sz = strxfrm(dst, source, 10);
  return 0;
}
