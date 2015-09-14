// RUN: test.sh -e -t %t %s

// Concatenate onto a destination string that is too short.
// This should cause an error that should be caught.

#include <string.h>

void do_cat(const char *src)
{
  char dest[10] = "";
  strcat(dest, src);
}

int main()
{
  char str[] = "This is over 10.";
  do_cat(str);
  return 0;
}
