// RUN: test.sh -p -t %t %s

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>

// Ensure that a correct use of realpath() is not flagged as an error.

char buffer[PATH_MAX];
int main()
{
  realpath ("/etc/passwd", buffer);
  return 0;
}
