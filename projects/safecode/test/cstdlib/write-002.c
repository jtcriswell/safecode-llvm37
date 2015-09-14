// RUN: test.sh -e -t %t %s
// XFAIL: darwin

#include <string.h>
#include <unistd.h>

// A use of write() causing a buffer overflow.

int main()
{
  int pipefd[2];
  char buf[200];

  memset(&buf[0], 0, 200);

  pipe(pipefd);

  write(pipefd[1], &buf[197], 4);

  return 0;
}
