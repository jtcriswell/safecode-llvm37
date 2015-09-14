// RUN: test.sh -e -t %t %s

// Concatenation destination and source overlap.

#include <string.h>

int main()
{
  char buf[10] = "afg";
  strcat(&buf[2], buf);
  return 0;
}
