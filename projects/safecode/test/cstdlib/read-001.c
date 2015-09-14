// RUN: test.sh -p -t %t %s

#include <assert.h>
#include <unistd.h>

// Ensure that a correct use of read() is not flagged as an error.

int main()
{
  int pipefd[2];
  char buf[1];

  pipe(pipefd);

  // Read nothing.
  assert(read(pipefd[0], &buf[0], 0) == 0);

  // Write a single character.
  write(pipefd[1], "C", 1);

  // Read back the same character.
  assert(read(pipefd[0], &buf[0], 1) == 1 && buf[0] == 'C');

  return 0;
}
