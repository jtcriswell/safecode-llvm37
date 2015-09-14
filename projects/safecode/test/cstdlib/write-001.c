// RUN: test.sh -p -t %t %s

#include <assert.h>
#include <unistd.h>

// Ensure that a correct use of write() is not flagged as an error.

int main()
{
  int pipefd[2];
  char buf[1];

  buf[0] = 'C';

  pipe(pipefd);

  // Write a single character.
  assert(write(pipefd[1], &buf[0], 1) == 1);

  return 0;
}
