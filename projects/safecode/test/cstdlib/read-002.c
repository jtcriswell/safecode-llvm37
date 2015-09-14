// RUN: test.sh -e -t %t %s
// XFAIL: darwin

#include <unistd.h>

// A use of read() causing a buffer overflow.

int main()
{
  int pipefd[2];
  char buf[200];

  pipe(pipefd);

  write(pipefd[1], "test", 4);
  read(pipefd[0], &buf[199], 2);

  return 0;
}
