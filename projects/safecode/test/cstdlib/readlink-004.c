// RUN: test.sh -e -t %t %s

#include <unistd.h>

// Ensure that a correct use of readlink() is not flagged as an error.

int main()
{
  char * buffer = malloc (8);
  readlink ("/etc/passwd", buffer, 24);
  return 0;
}
