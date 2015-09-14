// RUN: test.sh -e -t %t %s
// XFAIL: darwin

#include <unistd.h>

// A use of read() causing a buffer overflow.

int main()
{
  int pipefd[2];
  char buf[1];

  pipe(pipefd);

  write(pipefd[1], "test", 3);
  read(pipefd[0], &buf[0], 2);

  return 0;
}
