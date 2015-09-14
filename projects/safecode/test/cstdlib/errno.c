// RUN: test.sh -p -t %t %s
#include <unistd.h>
#include <errno.h>

int
main (int argc, char ** argv) {
  getpid();
  if (errno == ENOSYS)
    return 1;
  return 0;
}

