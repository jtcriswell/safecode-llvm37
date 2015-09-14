// RUN: test.sh -e -t %t %s

#include <unistd.h>
#include <stdlib.h>

// Ensure that a correct use of getenv() is not flagged as an error.

int main()
{
  char * p = getenv ("PATH");
  int index = strlen (p) + 10;
  printf ("%c\n", p[index]);
  return 0;
}
