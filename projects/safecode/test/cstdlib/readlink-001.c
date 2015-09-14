// RUN: test.sh -p -t %t %s

#include <limits.h>
#include <unistd.h>

// Ensure that a correct use of readlink() is not flagged as an error.

int main()
{
  char buffer[PATH_MAX];
  readlink ("/etc/passwd", buffer, PATH_MAX);
  return 0;
}
