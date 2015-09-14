// RUN: test.sh -p -t %t %s

#include <unistd.h>

// Ensure that a correct use of readlink() is not flagged as an error.

char buffer[24];

int main()
{
  readlink ("/etc/passwd", buffer, 24);
  return 0;
}
