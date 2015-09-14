// RUN: test.sh -e -t %t %s

// Concatenation onto unterminated destination string.

#include <string.h>

void do_cat(const char *src)
{
  char f[] = { 'A' };
  strcat(f, src);
}

int main()
{
  do_cat("");
  return 0;
}
