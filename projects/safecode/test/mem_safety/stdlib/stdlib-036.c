/* read into an invalid buffer */

#include <unistd.h>

#define BUFSZ sizeof(int)

void read_int(int *ptr, int fd)
{
  read(fd, ptr, sizeof(int));
}

int main()
{
  char buffer[BUFSZ];
  int  fds[2];
  void *ptr;
  int value;

  pipe(fds);
  value = 99;
  write(fds[1], &value, sizeof(int));

  ptr = &buffer[-1];
  read_int(ptr, fds[0]);

  close(fds[0]);
  close(fds[1]);
  return 0;
}
