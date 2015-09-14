// RUN: test.sh -p -t %t %s

#include <unistd.h>
#include <stdlib.h>

// Ensure that a correct use of realpath() is not flagged as an error.

int main()
{
  char * buffer = malloc (24);
  realpath ("/etc/passwd", buffer);
  return 0;
}
