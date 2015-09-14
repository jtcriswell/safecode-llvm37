// RUN: test.sh -p -t %t %s

#include <stdlib.h>
#include <unistd.h>

// Ensure that a correct use of realpath() is not flagged as an error.
// Note that getenv() should be treated as an allocator or should be considered
// as an external function

int main()
{
  char * buffer = malloc (24);
  if (buffer)
    realpath (getenv ("SHELL"), buffer);
  return 0;
}
