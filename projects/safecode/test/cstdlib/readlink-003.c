// RUN: test.sh -p -t %t %s

#include <unistd.h>

// Ensure that a correct use of readlink() is not flagged as an error.

int main()
{
  char * buffer = malloc (24);
  readlink ("/etc/passwd", buffer, 24);
  return 0;
}
